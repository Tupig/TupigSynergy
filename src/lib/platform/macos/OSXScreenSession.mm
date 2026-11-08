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

#pragma mark -

//
// FAST USER SWITCH NOTIFICATION SUPPORT
//
// OSXScreen::userSwitchCallback(void*)
//
// gets called if a fast user switch occurs
//

pascal OSStatus OSXScreen::userSwitchCallback(EventHandlerCallRef nextHandler, EventRef theEvent, void *inUserData)
{
  OSXScreen *screen = (OSXScreen *)inUserData;
  uint32_t kind = GetEventKind(theEvent);
  IEventQueue *events = screen->getEvents();

  if (kind == kEventSystemUserSessionDeactivated) {
    LOG_DEBUG("user session deactivated");
    events->addEvent(Event(EventTypes::ScreenSuspend, screen->getEventTarget()));
  } else if (kind == kEventSystemUserSessionActivated) {
    LOG_DEBUG("user session activated");
    events->addEvent(Event(EventTypes::ScreenResume, screen->getEventTarget()));
  }
  return (CallNextEventHandler(nextHandler, theEvent));
}

#pragma mark -

//
// SLEEP/WAKEUP NOTIFICATION SUPPORT
//
// OSXScreen::watchSystemPowerThread(void*)
//
// main of thread monitoring system power (sleep/wakup) using a CFRunLoop
//

void OSXScreen::watchSystemPowerThread(const void *)
{
  io_object_t notifier;
  IONotificationPortRef notificationPortRef;
  CFRunLoopSourceRef runloopSourceRef = 0;

  m_pmRunloop = CFRunLoopGetCurrent();
  // install system power change callback
  m_pmRootPort = IORegisterForSystemPower(this, &notificationPortRef, powerChangeCallback, &notifier);
  if (m_pmRootPort == 0) {
    LOG_WARN("IORegisterForSystemPower failed");
  } else {
    runloopSourceRef = IONotificationPortGetRunLoopSource(notificationPortRef);
    CFRunLoopAddSource(m_pmRunloop, runloopSourceRef, kCFRunLoopCommonModes);
  }

  // thread is ready
  {
    Lock lock(m_pmMutex);
    *m_pmThreadReady = true;
    m_pmThreadReady->signal();
  }

  // if we were unable to initialize then exit.  we must do this after
  // setting m_pmThreadReady to true otherwise the parent thread will
  // block waiting for it.
  if (m_pmRootPort == 0) {
    LOG_WARN("failed to init watchSystemPowerThread");
    return;
  }

  LOG_DEBUG("started watchSystemPowerThread");

  LOG_DEBUG("waiting for event loop");
  m_events->waitForReady();

  {
    Lock lockCarbon(m_carbonLoopMutex);
    if (*m_carbonLoopReady == false) {

      // we signalling carbon loop ready before starting
      // unless we know how to do it within the loop
      LOG_DEBUG("signalling carbon loop ready");

      *m_carbonLoopReady = true;
      m_carbonLoopReady->signal();
    }
  }

  // start the run loop
  LOG_DEBUG("starting carbon loop");
  CFRunLoopRun();
  LOG_DEBUG("carbon loop has stopped");

  // cleanup
  if (notificationPortRef) {
    CFRunLoopRemoveSource(m_pmRunloop, runloopSourceRef, kCFRunLoopDefaultMode);
    CFRunLoopSourceInvalidate(runloopSourceRef);
    CFRelease(runloopSourceRef);
  }

  Lock lock(m_pmMutex);
  IODeregisterForSystemPower(&notifier);
  m_pmRootPort = 0;
  LOG_DEBUG("stopped watchSystemPowerThread");
}

void OSXScreen::powerChangeCallback(void *refcon, io_service_t, natural_t messageType, void *messageArg)
{
  ((OSXScreen *)refcon)->handlePowerChangeRequest(messageType, messageArg);
}

void OSXScreen::handlePowerChangeRequest(natural_t messageType, void *messageArg)
{
  // we've received a power change notification
  switch (messageType) {
  case kIOMessageSystemWillSleep:
    // OSXScreen has to handle this in the main thread so we have to
    // queue a confirm sleep event here.  we actually don't allow the
    // system to sleep until the event is handled.
    m_events->addEvent(
        Event(EventTypes::OsxScreenConfirmSleep, getEventTarget(), messageArg, Event::EventFlags::DontFreeData)
    );
    return;

  case kIOMessageSystemHasPoweredOn:
    LOG_DEBUG("system wakeup");
    m_events->addEvent(Event(EventTypes::ScreenResume, getEventTarget()));
    break;

  default:
    break;
  }

  Lock lock(m_pmMutex);
  if (m_pmRootPort != 0) {
    IOAllowPowerChange(m_pmRootPort, (long)messageArg);
  }
}

void OSXScreen::handleConfirmSleep(const Event &event)
{
  long messageArg = (long)event.getData();
  if (messageArg != 0) {
    Lock lock(m_pmMutex);
    if (m_pmRootPort != 0) {
      // deliver suspend event immediately.
      m_events->addEvent(
          Event(EventTypes::ScreenSuspend, getEventTarget(), nullptr, Event::EventFlags::DeliverImmediately)
      );

      LOG_DEBUG("system will sleep");
      IOAllowPowerChange(m_pmRootPort, messageArg);
    }
  }
}

bool OSXScreen::checkAXPermissions()
{
  if (AXIsProcessTrusted()) {
    return true;
  }
  LOG_CRIT("process is not trusted anymore, quitting");
  disable();
  App &app = App::instance();
  app.getEvents()->addEvent(Event(EventTypes::Quit, nullptr, new ExitEventData(s_exitFailed)));
  return false;
}

std::string OSXScreen::getSecureInputApp() const
{
  if (IsSecureEventInputEnabled()) {
    int secureInputProcessPID = getSecureInputEventPID();
    if (secureInputProcessPID == 0)
      return "unknown";
    return getProcessName(secureInputProcessPID);
  }
  return "";
}

int getSecureInputEventPID()
{
  io_service_t service = MACH_PORT_NULL, service_root = MACH_PORT_NULL;
  mach_port_t masterPort;

  kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
  if (kr != KERN_SUCCESS)
    return 0;

  // IO registry refuses to tap into the root level directly
  // as a workaround access the parent of the top user level
  service = IORegistryEntryFromPath(masterPort, kIOServicePlane ":/");
  IORegistryEntryGetParentEntry(service, kIOServicePlane, &service_root);

  std::unique_ptr<std::remove_pointer<CFTypeRef>::type, decltype(&CFRelease)> consoleUsers(
      IORegistryEntrySearchCFProperty(
          service_root, kIOServicePlane, CFSTR("IOConsoleUsers"), nullptr,
          kIORegistryIterateParents | kIORegistryIterateRecursively
      ),
      CFRelease
  );
  if (!consoleUsers)
    return 0;

  CFTypeID type = CFGetTypeID(consoleUsers.get());
  if (type != CFArrayGetTypeID())
    return 0;

  CFTypeRef dict = CFArrayGetValueAtIndex((CFArrayRef)consoleUsers.get(), 0);
  if (!dict)
    return 0;

  type = CFGetTypeID(dict);
  if (type != CFDictionaryGetTypeID())
    return 0;

  CFTypeRef secureInputPID = nullptr;
  CFDictionaryGetValueIfPresent((CFDictionaryRef)dict, CFSTR("kCGSSessionSecureInputPID"), &secureInputPID);

  if (secureInputPID == nullptr)
    return 0;

  type = CFGetTypeID(secureInputPID);
  if (type != CFNumberGetTypeID())
    return 0;

  auto pidRef = (CFNumberRef)secureInputPID;
  CFNumberType numberType = CFNumberGetType(pidRef);
  if (numberType != kCFNumberSInt32Type)
    return 0;

  int pid;
  CFNumberGetValue(pidRef, kCFNumberSInt32Type, &pid);
  return pid;
}

std::string getProcessName(int pid)
{
  if (!pid)
    return "";
  char buf[128];
  proc_name(pid, buf, sizeof(buf));
  return buf;
}

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

void setZeroSuppressionInterval()
{
  CGSetLocalEventsSuppressionInterval(0.0);
}

void logCursorVisibility()
{
  // CGCursorIsVisible is probably deprecated because its unreliable.
  if (!CGCursorIsVisible()) {
    LOG_WARN("cursor may not be visible");
  }
}

void avoidHesitatingCursor()
{
  // This used to be necessary to get smooth mouse motion on other screens,
  // but now is just to avoid a hesitating cursor when transitioning to
  // the primary (this) screen.
  CGSetLocalEventsSuppressionInterval(0.0001);
}

#pragma GCC diagnostic error "-Wdeprecated-declarations"

#pragma clang diagnostic pop
