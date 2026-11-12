/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXScreen.h"

#include "ClientApp.h"
#include "Clipboard.h"
#include "DisplayInvalidException.h"
#include "KeyMap.h"
#include "OSXClipboard.h"
#include "OSXEventQueueBuffer.h"
#include "OSXKeyState.h"
#include "OSXMediaKeySupport.h"
#include "OSXPasteboardPeeker.h"
#include "OSXScreenSaver.h"
#include "arch/Arch.h"
#include "arch/ArchException.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/TMethodJob.h"
#include "client/Client.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "mt/CondVar.h"
#include "mt/Lock.h"
#include "mt/Mutex.h"
#include "mt/Thread.h"

#include <AppKit/NSEvent.h>
#include <AvailabilityMacros.h>
#include <IOKit/hidsystem/event_status_driver.h>
#include <cstring>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <mach-o/dyld.h>
#include <math.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

int getSecureInputEventPID();
std::string getProcessName(int pid);

// TODO: upgrade deprecated function usage in these functions.
void setZeroSuppressionInterval();
void avoidSupression();
void logCursorVisibility();
void avoidHesitatingCursor();

//
// OSXScreen
//

// The following creates a section that tells Mac OS X
// that it is OK to let us inject input in the login screen.
// Just the name of the section is important, not its contents.
__attribute__((used)) __attribute__((section("__CGPreLoginApp,__cgpreloginapp"))) static const char magic_section[] =
    "";

OSXScreen::OSXScreen(IEventQueue *events, bool isPrimary, bool enableLangSync)
    : PlatformScreen(events),
      m_isPrimary(isPrimary),
      m_isOnScreen(m_isPrimary),
      m_cursorPosValid(false),
      MouseButtonEventMap(NumButtonIDs),
      m_cursorHidden(false),
      m_keyState(nullptr),
      m_sequenceNumber(0),
      m_screensaver(nullptr),
      m_screensaverNotify(false),
      m_ownClipboard(false),
      m_clipboardTimer(nullptr),
      m_axTimer(nullptr),
      m_hiddenWindow(nullptr),
      m_userInputWindow(nullptr),
      m_switchEventHandlerRef(0),
      m_pmMutex(new Mutex),
      m_pmWatchThread(nullptr),
      m_pmThreadReady(new CondVar<bool>(m_pmMutex, false)),
      m_pmRootPort(0),
      m_activeModifierHotKey(0),
      m_activeModifierHotKeyMask(0),
      m_eventTapPort(nullptr),
      m_eventTapRLSR(nullptr),
      m_lastClickTime(0),
      m_clickState(1),
      m_lastSingleClickXCursor(0),
      m_lastSingleClickYCursor(0),
      m_events(events),
      m_impl(nullptr)
{
  m_displayID = CGMainDisplayID();
  if (!updateScreenShape(m_displayID, 0)) {
    throw DisplayInvalidException("failed to initialize screen shape");
  }

  try {
    m_screensaver = new OSXScreenSaver(m_events, getEventTarget());
    m_keyState = new OSXKeyState(m_events, AppUtil::instance().getKeyboardLayoutList(), enableLangSync);

    if (Settings::value(Settings::Core::PreventSleep).toBool()) {
      m_powerManager.disableSleep();
    }

    // only needed when running as a server.
    if (m_isPrimary) {
      // we can't pass options to show the dialog, this must be done by the gui.
      if (!AXIsProcessTrusted()) {
        throw std::runtime_error("assistive devices does not trust this process, allow it in system settings.");
      }
    }

    // install display manager notification handler
    CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback, this);

    // install fast user switching event handler
    EventTypeSpec switchEventTypes[2];
    switchEventTypes[0].eventClass = kEventClassSystem;
    switchEventTypes[0].eventKind = kEventSystemUserSessionDeactivated;
    switchEventTypes[1].eventClass = kEventClassSystem;
    switchEventTypes[1].eventKind = kEventSystemUserSessionActivated;
    EventHandlerUPP switchEventHandler = NewEventHandlerUPP(userSwitchCallback);
    InstallApplicationEventHandler(switchEventHandler, 2, switchEventTypes, this, &m_switchEventHandlerRef);
    DisposeEventHandlerUPP(switchEventHandler);

    constructMouseButtonEventMap();

    // watch for requests to sleep
    m_events->addHandler(EventTypes::OsxScreenConfirmSleep, getEventTarget(), [this](const auto &e) {
      handleConfirmSleep(e);
    });

    // create thread for monitoring system power state.
    *m_pmThreadReady = false;
    m_carbonLoopMutex = new Mutex();
    m_carbonLoopReady = new CondVar<bool>(m_carbonLoopMutex, false);
    LOG_DEBUG("starting watchSystemPowerThread");
    m_pmWatchThread = new Thread(new TMethodJob<OSXScreen>(this, &OSXScreen::watchSystemPowerThread));
  } catch (...) {
    m_events->removeHandler(EventTypes::OsxScreenConfirmSleep, getEventTarget());
    if (m_switchEventHandlerRef != 0) {
      RemoveEventHandler(m_switchEventHandlerRef);
    }

    CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallback, this);

    delete m_keyState;
    delete m_screensaver;
    throw;
  }

  // install event handlers
  m_events->addHandler(EventTypes::System, m_events->getSystemTarget(), [this](const auto &e) {
    handleSystemEvent(e);
  });

  // install the platform event queue
  m_events->adoptBuffer(new OSXEventQueueBuffer(m_events));
}

OSXScreen::~OSXScreen()
{
  disable();

  m_events->adoptBuffer(nullptr);
  m_events->removeHandler(EventTypes::System, m_events->getSystemTarget());

  if (m_pmWatchThread) {
    // make sure the thread has setup the runloop.
    {
      Lock lock(m_pmMutex);
      while (!(bool)*m_pmThreadReady) {
        m_pmThreadReady->wait();
      }
    }

    // now exit the thread's runloop and wait for it to exit
    LOG_DEBUG("stopping watchSystemPowerThread");
    CFRunLoopStop(m_pmRunloop);
    m_pmWatchThread->wait();
    delete m_pmWatchThread;
    m_pmWatchThread = nullptr;
  }
  delete m_pmThreadReady;
  delete m_pmMutex;

  m_events->removeHandler(EventTypes::OsxScreenConfirmSleep, getEventTarget());

  RemoveEventHandler(m_switchEventHandlerRef);

  CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallback, this);

  delete m_keyState;
  delete m_screensaver;

  delete m_carbonLoopMutex;
  delete m_carbonLoopReady;
}

void *OSXScreen::getEventTarget() const
{
  return const_cast<OSXScreen *>(this);
}

bool OSXScreen::getClipboard(ClipboardID, IClipboard *dst) const
{
  Clipboard::copy(dst, &m_pasteboard);
  return true;
}

void OSXScreen::getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const
{
  x = m_x;
  y = m_y;
  w = m_w;
  h = m_h;
}

void OSXScreen::getCursorPos(int32_t &x, int32_t &y) const
{
  CGEventRef event = CGEventCreate(nullptr);
  CGPoint mouse = CGEventGetLocation(event);
  x = mouse.x;
  y = mouse.y;
  m_cursorPosValid = true;
  m_xCursor = x;
  m_yCursor = y;
  CFRelease(event);
}

void OSXScreen::reconfigure(uint32_t activeSides)
{
  const static auto sidesText = sidesMaskToString(activeSides);
  LOG_DEBUG("active sides: %s (0x%02x)", sidesText.c_str(), activeSides);
  m_activeSides = activeSides;
}

uint32_t OSXScreen::activeSides()
{
  return m_activeSides;
}

void OSXScreen::warpCursor(int32_t x, int32_t y)
{
  if (m_eventTapRunLoop && CFRunLoopGetCurrent() != m_eventTapRunLoop) {
    CFRunLoopPerformBlock(m_eventTapRunLoop, kCFRunLoopDefaultMode, ^{
      warpCursor(x, y);
    });
    CFRunLoopWakeUp(m_eventTapRunLoop);
    return;
  }

  // move cursor without generating events
  CGPoint pos;
  pos.x = x;
  pos.y = y;
  CGWarpMouseCursorPosition(pos);

  // save new cursor position
  m_xCursor = x;
  m_yCursor = y;
  m_cursorPosValid = true;
}

void OSXScreen::fakeInputBegin()
{
  // FIXME -- not implemented
}

void OSXScreen::fakeInputEnd()
{
  // FIXME -- not implemented
}

int32_t OSXScreen::getJumpZoneSize() const
{
  return 1;
}

bool OSXScreen::isAnyMouseButtonDown(uint32_t &buttonID) const
{
  if (m_buttonState.test(0)) {
    buttonID = kButtonLeft;
    return true;
  }

  return (GetCurrentButtonState() != 0);
}

void OSXScreen::getCursorCenter(int32_t &x, int32_t &y) const
{
  x = m_xCenter;
  y = m_yCenter;
}

void OSXScreen::enable()
{
  // watch the clipboard
  m_clipboardTimer = m_events->newTimer(1.0, nullptr);
  m_events->addHandler(EventTypes::Timer, m_clipboardTimer, [this](const auto &) { checkClipboards(); });

  m_axTimer = m_events->newTimer(1.0, nullptr);
  m_events->addHandler(EventTypes::Timer, m_axTimer, [this](const auto &) { checkAXPermissions(); });

  if (m_isPrimary) {
    // FIXME -- start watching jump zones

    // kCGEventTapOptionDefault = 0x00000000 (Missing in 10.4, so specified literally)
    m_eventTapPort = CGEventTapCreate(
        kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, kCGEventMaskForAllEvents, handleCGInputEvent,
        this
    );
  } else {
    // FIXME -- prevent system from entering power save mode

    hideCursor();

    // warp the mouse to the cursor center
    fakeMouseMove(m_xCenter, m_yCenter);

    // there may be a better way to do this, but we register an event handler even if we're
    // not on the primary display (acting as a client). This way, if a local event comes in
    // (either keyboard or mouse), we can make sure to show the cursor if we've hidden it.
    m_eventTapPort = CGEventTapCreate(
        kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, kCGEventMaskForAllEvents,
        handleCGInputEventSecondary, this
    );
  }

  if (m_eventTapPort) {
    m_eventTapRLSR = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_eventTapPort, 0);
    if (m_eventTapRLSR) {
      // Run the event tap on a dedicated thread with its own CFRunLoop so it fires
      // independently of whatever event loop the calling thread runs (e.g. QCoreApplication).
      // Use a semaphore to ensure m_eventTapRunLoop is set before enable() returns.
      auto sem = dispatch_semaphore_create(0);
      m_eventTapThread = std::thread([this, sem]() {
        m_eventTapRunLoop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(m_eventTapRunLoop, m_eventTapRLSR, kCFRunLoopDefaultMode);
        dispatch_semaphore_signal(sem);
        CFRunLoopRun();
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), m_eventTapRLSR, kCFRunLoopDefaultMode);
        m_eventTapRunLoop = nullptr;
      });
      dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
      dispatch_release(sem);
    } else {
      LOG_ERR("failed to create a CFRunLoopSourceRef for the quartz event tap");
    }
  } else {
    LOG_ERR("failed to create quartz event tap");
  }
}

void OSXScreen::disable()
{
  showCursor();

  // FIXME -- stop watching jump zones, stop capturing input

  if (m_eventTapRunLoop) {
    CFRunLoopStop(m_eventTapRunLoop);
  }
  if (m_eventTapThread.joinable()) {
    m_eventTapThread.join();
  }

  if (m_eventTapRLSR) {
    CFRelease(m_eventTapRLSR);
    m_eventTapRLSR = nullptr;
  }

  if (m_eventTapPort) {
    CGEventTapEnable(m_eventTapPort, false);
    CFRelease(m_eventTapPort);
    m_eventTapPort = nullptr;
  }
  // FIXME -- allow system to enter power saving mode

  if (m_clipboardTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_clipboardTimer);
    m_events->deleteTimer(m_clipboardTimer);
    m_clipboardTimer = nullptr;
  }

  if (m_axTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_axTimer);
    m_events->deleteTimer(m_axTimer);
    m_axTimer = nullptr;
  }

  m_isOnScreen = m_isPrimary;
}

void OSXScreen::enter()
{
  m_isOnScreen = true;
  showCursor();

  if (m_isPrimary) {
    setZeroSuppressionInterval();
  } else {
    // reset buttons
    m_buttonState.reset();

    // wakes the client screen
    io_registry_entry_t entry =
        IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/IOResources/IODisplayWrangler");

    if (entry != MACH_PORT_NULL) {
      IORegistryEntrySetCFProperty(entry, CFSTR("IORequestIdle"), kCFBooleanFalse);
      IOObjectRelease(entry);
    }

    avoidSupression();
  }
}

bool OSXScreen::canLeave()
{
  return true;
}

void OSXScreen::leave()
{
  hideCursor();

  if (m_isPrimary) {
    avoidHesitatingCursor();
    LOG_VERBOSE("centering cursor on leave: %+d, %+d", m_xCenter, m_yCenter);
    warpCursor(m_xCenter, m_yCenter);
  }

  // now off screen
  m_isOnScreen = false;
}

std::vector<std::string> OSXScreen::takeDraggingPaths()
{
  // Reads NSPasteboardNameDrag. Not runtime-verified on this machine (no macOS
  // host); the peeker itself is the restored upstream path, fixed to return a
  // CFArray so multi-file drags are not truncated at the first embedded NUL.
  std::vector<std::string> paths;

  CFArrayRef files = copyDraggedFilePaths();
  if (files == nullptr) {
    return paths;
  }

  const CFIndex count = CFArrayGetCount(files);
  paths.reserve(static_cast<size_t>(count));

  for (CFIndex i = 0; i < count; ++i) {
    auto *path = static_cast<CFStringRef>(CFArrayGetValueAtIndex(files, i));
    if (path == nullptr) {
      continue;
    }

    const CFIndex length = CFStringGetLength(path);
    const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string utf8(static_cast<size_t>(maxSize), '\0');
    if (CFStringGetCString(path, utf8.data(), maxSize, kCFStringEncodingUTF8)) {
      utf8.resize(std::strlen(utf8.c_str()));
      if (!utf8.empty()) {
        paths.push_back(std::move(utf8));
      }
    }
  }

  CFRelease(files);
  return paths;
}

bool OSXScreen::setClipboard(ClipboardID, const IClipboard *src)
{
  if (src != nullptr) {
    LOG_DEBUG("setting clipboard");
    Clipboard::copy(&m_pasteboard, src);
  }
  return true;
}

void OSXScreen::checkClipboards()
{
  LOG_VERBOSE("checking clipboard");
  if (m_pasteboard.synchronize()) {
    LOG_DEBUG("clipboard changed");
    sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardClipboard);
    sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardSelection);
  }
}

void OSXScreen::openScreensaver(bool notify)
{
  m_screensaverNotify = notify;
  if (!m_screensaverNotify) {
    m_screensaver->disable();
  }
}

void OSXScreen::closeScreensaver()
{
  if (!m_screensaverNotify) {
    m_screensaver->enable();
  }
}

void OSXScreen::screensaver(bool activate)
{
  if (activate) {
    m_screensaver->activate();
  } else {
    m_screensaver->deactivate();
  }
}

void OSXScreen::resetOptions()
{
  // no options
}

void OSXScreen::setOptions(const OptionsList &)
{
  // no options
}

void OSXScreen::setSequenceNumber(uint32_t seqNum)
{
  m_sequenceNumber = seqNum;
}

bool OSXScreen::isPrimary() const
{
  return m_isPrimary;
}

void OSXScreen::updateButtons()
{
  uint32_t buttons = GetCurrentButtonState();

  m_buttonState.overwrite(buttons);
}

IKeyState *OSXScreen::getKeyState() const
{
  return m_keyState;
}

bool OSXScreen::updateScreenShape(const CGDirectDisplayID, const CGDisplayChangeSummaryFlags)
{
  return updateScreenShape();
}

bool OSXScreen::updateScreenShape()
{
  // get info for each display
  CGDisplayCount displayCount = 0;

  if (CGGetActiveDisplayList(0, nullptr, &displayCount) != CGDisplayNoErr) {
    return false;
  }

  if (displayCount == 0) {
    return false;
  }

  CGDirectDisplayID *displays = new CGDirectDisplayID[displayCount];
  if (displays == nullptr) {
    return false;
  }

  if (CGGetActiveDisplayList(displayCount, displays, &displayCount) != CGDisplayNoErr) {
    delete[] displays;
    return false;
  }

  // get smallest rect enclosing all display rects
  CGRect totalBounds = CGRectZero;
  for (CGDisplayCount i = 0; i < displayCount; ++i) {
    CGRect bounds = CGDisplayBounds(displays[i]);
    totalBounds = CGRectUnion(totalBounds, bounds);
  }

  // get shape of default screen
  m_x = (int32_t)totalBounds.origin.x;
  m_y = (int32_t)totalBounds.origin.y;
  m_w = (int32_t)totalBounds.size.width;
  m_h = (int32_t)totalBounds.size.height;

  // get center of default screen
  CGDirectDisplayID main = CGMainDisplayID();
  const CGRect rect = CGDisplayBounds(main);
  m_xCenter = (rect.origin.x + rect.size.width) / 2;
  m_yCenter = (rect.origin.y + rect.size.height) / 2;

  delete[] displays;
  // We want to notify the peer screen whether we are primary screen or not
  sendEvent(EventTypes::ScreenShapeChanged);

  LOG_DEBUG(
      "screen shape: center=%d,%d size=%dx%d on %u %s", m_x, m_y, m_w, m_h, displayCount,
      (displayCount == 1) ? "display" : "displays"
  );

  return true;
}

char *OSXScreen::CFStringRefToUTF8String(CFStringRef aString)
{
  if (aString == nullptr) {
    return nullptr;
  }

  CFIndex length = CFStringGetLength(aString);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
  char *buffer = (char *)malloc(maxSize);

  if (!CFStringGetCString(aString, buffer, maxSize, kCFStringEncodingUTF8)) {
    free(buffer);
    buffer = nullptr;
  }

  return buffer;
}

void OSXScreen::waitForCarbonLoop() const
{
  if (*m_carbonLoopReady) {
    LOG_DEBUG("carbon loop already ready");
    return;
  }

  Lock lock(m_carbonLoopMutex);

  LOG_DEBUG("waiting for carbon loop");

  // CondVar::wait() reports only a signal received while waiting, so check the
  // stored value after taking the lock: the session thread may have signalled
  // between the fast-path check above and this lock, and waiting on the return
  // value alone would then block forever.
  while (!*m_carbonLoopReady) {
    m_carbonLoopReady->wait();
  }

  LOG_DEBUG("carbon loop ready");
}

void avoidSupression()
{
  // avoid suppression of local hardware events
  CGSetLocalEventsFilterDuringSupressionState(
      kCGEventFilterMaskPermitAllEvents, kCGEventSupressionStateSupressionInterval
  );
  CGSetLocalEventsFilterDuringSupressionState(
      (kCGEventFilterMaskPermitLocalKeyboardEvents | kCGEventFilterMaskPermitSystemDefinedEvents),
      kCGEventSupressionStateRemoteMouseDrag
  );
}

#pragma clang diagnostic pop
