/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/AppStyle.h"
#include "gui/Motion.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/widgets/PageHost.h"
#include <QAccessible>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QtTest>
#include <array>
#include <memory>

namespace {
enum class ButtonTraceEvent
{
  Activation,
  Pressed,
  Released,
  Clicked,
  CaptureEntry
};

struct ButtonOverlaySnapshot
{
  bool present = false;
  bool visible = false;
  QRect geometry;
  bool animationPresent = false;
  bool recursiveLookupPresent = false;
  bool recursiveLookupMatches = false;
  int state = -1;
  int currentTime = -1;
  qreal value = -1;
  qreal opacity = -1;
};

ButtonOverlaySnapshot buttonOverlaySnapshot(QWidget &button)
{
  ButtonOverlaySnapshot result;
  const QPointer<QWidget> overlay = button.findChild<QWidget *>("buttonFeedbackOverlay", Qt::FindDirectChildrenOnly);
  const QPointer<QVariantAnimation> recursive = button.findChild<QVariantAnimation *>("buttonFeedbackAnimation");
  result.recursiveLookupPresent = !recursive.isNull();
  if (!overlay)
    return result;
  result.present = true;
  result.visible = overlay->isVisible();
  result.geometry = overlay->geometry();
  result.opacity = overlay->property("motionOpacity").toDouble();
  const QPointer<QVariantAnimation> animation =
      overlay->findChild<QVariantAnimation *>("buttonFeedbackAnimation", Qt::FindDirectChildrenOnly);
  result.animationPresent = !animation.isNull();
  result.recursiveLookupMatches = animation && recursive == animation;
  if (animation) {
    result.state = static_cast<int>(animation->state());
    result.currentTime = animation->currentTime();
    result.value = animation->currentValue().toDouble();
  }
  return result;
}

struct ButtonFrameTrace : QObject
{
  struct Event
  {
    ButtonTraceEvent kind = ButtonTraceEvent::Activation;
    qint64 milliseconds = -1;
  };
  struct Grab
  {
    qint64 beforeMilliseconds = -1;
    qint64 afterMilliseconds = -1;
    ButtonOverlaySnapshot before;
    ButtonOverlaySnapshot after;
  };
  explicit ButtonFrameTrace(QWidget *button) : target(button)
  {
  }

  void begin()
  {
    clock.start();
    collecting = true;
    record(ButtonTraceEvent::Activation);
  }
  void record(ButtonTraceEvent kind)
  {
    if (!collecting)
      return;
    if (eventCount == static_cast<int>(events.size())) {
      eventOverflow = true;
      return;
    }
    events.at(static_cast<std::size_t>(eventCount++)) = {kind, clock.elapsed()};
  }
  void beforeGrab(QWidget &widget)
  {
    if (!collecting || target.data() != &widget)
      return;
    if (grabCount == static_cast<int>(grabs.size())) {
      grabOverflow = true;
      return;
    }
    auto &grab = grabs.at(static_cast<std::size_t>(grabCount));
    grab.before = buttonOverlaySnapshot(widget);
    grab.beforeMilliseconds = clock.elapsed();
  }
  void afterGrab(QWidget &widget)
  {
    if (!collecting || target.data() != &widget || grabCount == static_cast<int>(grabs.size()))
      return;
    auto &grab = grabs.at(static_cast<std::size_t>(grabCount++));
    grab.afterMilliseconds = clock.elapsed();
    grab.after = buttonOverlaySnapshot(widget);
  }

  QPointer<QWidget> target;
  QElapsedTimer clock;
  std::array<Event, 16> events;
  std::array<Grab, 13> grabs;
  int eventCount = 0;
  int grabCount = 0;
  bool eventOverflow = false;
  bool grabOverflow = false;
  bool collecting = false;
};

QPointer<ButtonFrameTrace> activeButtonFrameTrace;

QStackedWidget *prepareHost(PageHost &host)
{
  auto *stack = new QStackedWidget(&host);
  stack->setObjectName("pageStack");
  auto *home = new QWidget;
  home->setStyleSheet("background: #112244;");
  stack->addWidget(home);
  auto *layout = new QVBoxLayout(&host);
  layout->addWidget(stack);
  host.resize(600, 460);
  host.setAttribute(Qt::WA_DontShowOnScreen);
  host.show();
  QCoreApplication::processEvents();
  return stack;
}
QDialog *page(const QString &color)
{
  auto *dialog = new QDialog;
  auto *layout = new QVBoxLayout(dialog);
  auto *label = new QLabel("Motion fixture content");
  label->setMinimumHeight(140);
  layout->addWidget(label);
  dialog->setStyleSheet("background:" + color + ";");
  return dialog;
}
QList<QImage> collectFrames(QWidget &widget, int milliseconds = 240)
{
  QList<QImage> result;
  if (activeButtonFrameTrace)
    activeButtonFrameTrace->beforeGrab(widget);
  result << widget.grab().toImage();
  if (activeButtonFrameTrace)
    activeButtonFrameTrace->afterGrab(widget);
  for (int elapsed = 0; elapsed < milliseconds; elapsed += 15) {
    QTest::qWait(15);
    if (activeButtonFrameTrace)
      activeButtonFrameTrace->beforeGrab(widget);
    result << widget.grab().toImage();
    if (activeButtonFrameTrace)
      activeButtonFrameTrace->afterGrab(widget);
  }
  return result;
}
bool hasIntermediate(const QList<QImage> &frames)
{
  if (frames.size() < 3)
    return false;
  for (int i = 1; i + 1 < frames.size(); ++i)
    if (frames[i] != frames.first() && frames[i] != frames.last())
      return true;
  return false;
}
void saveFrames(const QList<QImage> &frames, const QString &name)
{
  const auto path = qEnvironmentVariable("SYNERGY_R3_EVIDENCE_DIR");
  if (path.isEmpty())
    return;
  QDir().mkpath(path + "/motion");
  for (int i = 0; i < frames.size(); ++i)
    frames[i].save(path + "/motion/" + name + QStringLiteral("-%1.png").arg(i, 2, 10, QLatin1Char('0')));
}

qint64 differentFramePixels(const QImage &left, const QImage &right)
{
  if (left.isNull() || right.isNull() || left.size() != right.size() ||
      left.devicePixelRatio() != right.devicePixelRatio())
    return -1;
  const auto a = left.convertToFormat(QImage::Format_ARGB32);
  const auto b = right.convertToFormat(QImage::Format_ARGB32);
  qint64 different = 0;
  for (int y = 0; y < a.height(); ++y) {
    const auto *aRow = reinterpret_cast<const QRgb *>(a.constScanLine(y));
    const auto *bRow = reinterpret_cast<const QRgb *>(b.constScanLine(y));
    for (int x = 0; x < a.width(); ++x)
      different += aRow[x] != bRow[x];
  }
  return different;
}

void writeOverlaySnapshot(QDebug &output, const char *label, const ButtonOverlaySnapshot &snapshot)
{
  output << ' ' << label << "={present:" << snapshot.present << ",visible:" << snapshot.visible
         << ",geometry:" << snapshot.geometry << ",animation:" << snapshot.animationPresent
         << ",lookupPresent:" << snapshot.recursiveLookupPresent << ",lookupSame:" << snapshot.recursiveLookupMatches
         << ",state:" << snapshot.state << ",time:" << snapshot.currentTime << ",value:" << snapshot.value
         << ",opacity:" << snapshot.opacity << '}';
}

void summarizeButtonFrames(const ButtonFrameTrace &trace, const QList<QImage> &frames, const QString &activation)
{
  const int count = qMin(static_cast<int>(frames.size()), static_cast<int>(trace.grabs.size()));
  int unique = 0;
  for (int index = 0; index < count; ++index) {
    bool seen = false;
    for (int earlier = 0; earlier < index; ++earlier)
      seen = seen || frames.at(index) == frames.at(earlier);
    unique += !seen;
  }
  qInfo().nospace() << "MD1 summary activation=" << activation << " qt=" << qVersion()
                    << " platform=" << QGuiApplication::platformName() << " frames=" << frames.size()
                    << " traced=" << trace.grabCount << " events=" << trace.eventCount
                    << " eventOverflow=" << trace.eventOverflow << " grabOverflow=" << trace.grabOverflow
                    << " uniqueByQImageEquality=" << unique << " intermediateByOriginalRule=" << hasIntermediate(frames)
                    << " incomparablePixelDifference=-1";
  static constexpr std::array<const char *, 5> names{"activation", "pressed", "released", "clicked", "capture-entry"};
  for (int index = 0; index < trace.eventCount; ++index) {
    const auto &event = trace.events.at(static_cast<std::size_t>(index));
    qInfo().nospace() << "MD1 event activation=" << activation
                      << " kind=" << names.at(static_cast<std::size_t>(event.kind)) << " ms=" << event.milliseconds;
  }
  for (int index = 0; index < count; ++index) {
    const auto &image = frames.at(index);
    const auto &grab = trace.grabs.at(static_cast<std::size_t>(index));
    auto output = qInfo().nospace();
    output << "MD1 frame activation=" << activation << " index=" << index << " size=" << image.size()
           << " dpr=" << image.devicePixelRatio() << " grabBeforeMs=" << grab.beforeMilliseconds
           << " grabAfterMs=" << grab.afterMilliseconds
           << " differentFromFirst=" << differentFramePixels(image, frames.first())
           << " differentFromLast=" << differentFramePixels(image, frames.last())
           << " differentFromPrevious=" << (index == 0 ? -1 : differentFramePixels(image, frames.at(index - 1)));
    writeOverlaySnapshot(output, "before", grab.before);
    writeOverlaySnapshot(output, "after", grab.after);
  }
}
} // namespace
class R3MotionContractTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init()
  {
    synergy::gui::setMotionModeForTesting(synergy::gui::MotionMode::Animated);
  }
  void cleanup()
  {
    synergy::gui::setMotionModeForTesting(synergy::gui::MotionMode::Reduced);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
  void pagePushAndBackHaveNonEndpointFrames();
  void buttonStandardActivation_data();
  void buttonStandardActivation();
  void buttonFeedbackRendersDeterministicInteriorFrame_data();
  void buttonFeedbackRendersDeterministicInteriorFrame();
  void contentTransitionBoundsRenderedBrightness();
  void tabAndOptionTransitionsPaint();
  void interruptionResizeBackAndDestruction();
  void reducedMotionSettlesImmediately();
};
void R3MotionContractTests::pagePushAndBackHaveNonEndpointFrames()
{
  PageHost host;
  auto *stack = prepareHost(host);
  const auto geometry = host.geometry();
  auto *editor = page("#aa4422");
  host.push(editor);
  auto *animation = host.findChild<QVariantAnimation *>("contentTransitionAnimation");
  QVERIFY(animation);
  QCOMPARE(animation->duration(), 160);
  auto *overlay = host.findChild<QWidget *>("contentTransitionOverlay");
  QVERIFY(overlay);
  QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
  const auto frames = collectFrames(host);
  saveFrames(frames, "page-push");
  QVERIFY2(hasIntermediate(frames), "Rendered page push must include a frame distinct from both endpoints");
  QCOMPARE(host.geometry(), geometry);
  QVERIFY(host.hasEditor());
  QVERIFY(stack->currentWidget() != stack->widget(0));
  const auto finalHeight = host.preferredContentHeight(host.width());
  QSignalSpy rejected(editor, &QDialog::rejected);
  editor->reject();
  QCOMPARE(rejected.count(), 1);
  const auto backFrames = collectFrames(host);
  saveFrames(backFrames, "page-back");
  QVERIFY2(hasIntermediate(backFrames), "Back must paint actual intermediate frames");
  QCOMPARE(stack->currentIndex(), 0);
  QVERIFY(!host.hasEditor());
  QCOMPARE(host.geometry(), geometry);
  QVERIFY(finalHeight > 0);
}
void R3MotionContractTests::buttonStandardActivation_data()
{
  QTest::addColumn<QString>("activation");
  for (const char *name : {"mouse", "Space", "Return", "Enter", "accessible-press", "click"})
    QTest::newRow(name) << QString::fromLatin1(name);
}
void R3MotionContractTests::buttonStandardActivation()
{
  QFETCH(QString, activation);
  QDialog dialog;
  auto *layout = new QVBoxLayout(&dialog);
  auto *button = new QPushButton("Activate once");
  button->setDefault(true);
  layout->addWidget(button);
  dialog.resize(240, 100);
  dialog.setAttribute(Qt::WA_DontShowOnScreen);
  dialog.show();
  button->setFocus();
  QCoreApplication::processEvents();
  const auto geometry = button->geometry();
  auto *focus = QApplication::focusWidget();
  QSignalSpy clicked(button, &QPushButton::clicked);
  ButtonFrameTrace trace(button);
  QScopedValueRollback<QPointer<ButtonFrameTrace>> traceHook(
      activeButtonFrameTrace, QPointer<ButtonFrameTrace>(&trace)
  );
  // Destroy the connection context and restore the hook before captured state.
  QObject traceContext;
  connect(button, &QPushButton::pressed, &traceContext, [&trace] { trace.record(ButtonTraceEvent::Pressed); });
  connect(button, &QPushButton::released, &traceContext, [&trace] { trace.record(ButtonTraceEvent::Released); });
  connect(button, &QPushButton::clicked, &traceContext, [&trace] { trace.record(ButtonTraceEvent::Clicked); });
  if (activation != "accessible-press")
    trace.begin();
  if (activation == "mouse")
    QTest::mouseClick(button, Qt::LeftButton);
  else if (activation == "Space")
    QTest::keyClick(button, Qt::Key_Space);
  else if (activation == "Return")
    QTest::keyClick(button, Qt::Key_Return);
  else if (activation == "Enter")
    QTest::keyClick(button, Qt::Key_Enter);
  else if (activation == "accessible-press") {
    // Qt may implement AX press using animateClick and its intrinsic release delay.
    // Compare stock scheduling under Reduced, then require Animated adds no delay.
    struct ActivationTiming
    {
      QElapsedTimer clock;
      qint64 pressed = -1;
      qint64 released = -1;
      qint64 clicked = -1;
      qint64 actionReturned = -1;
      qint64 observed = -1;
    };
    struct WaitObservation
    {
      const char *mode;
      ActivationTiming &timing;
      ~WaitObservation()
      {
        timing.observed = timing.clock.elapsed();
        qInfo().nospace() << "AX1 mode=" << mode << " pressedMs=" << timing.pressed << " releasedMs=" << timing.released
                          << " clickedMs=" << timing.clicked << " actionReturnedMs=" << timing.actionReturned
                          << " observedMs=" << timing.observed;
      }
    };
    ActivationTiming baselineTiming;
    ActivationTiming animatedTiming;
    // Connections must end before their captured timing state, while the button survives this branch.
    QObject timingContext;
    const auto recordSignals = [&timingContext](QPushButton *target, ActivationTiming &timing) {
      connect(target, &QPushButton::pressed, &timingContext, [&timing] {
        if (timing.pressed < 0)
          timing.pressed = timing.clock.elapsed();
      });
      connect(target, &QPushButton::released, &timingContext, [&timing] {
        if (timing.released < 0)
          timing.released = timing.clock.elapsed();
      });
      connect(target, &QPushButton::clicked, &timingContext, [&timing] {
        if (timing.clicked < 0)
          timing.clicked = timing.clock.elapsed();
      });
    };
    synergy::gui::setMotionModeForTesting(synergy::gui::MotionMode::Reduced);
    QPushButton baseline("Stock accessible baseline");
    baseline.setAttribute(Qt::WA_DontShowOnScreen);
    baseline.show();
    auto *baselineInterface = QAccessible::queryAccessibleInterface(&baseline);
    QVERIFY(baselineInterface && baselineInterface->actionInterface());
    QSignalSpy baselineClicked(&baseline, &QPushButton::clicked);
    recordSignals(&baseline, baselineTiming);
    baselineTiming.clock.start();
    baselineInterface->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    baselineTiming.actionReturned = baselineTiming.clock.elapsed();
    {
      WaitObservation observation{"stock", baselineTiming};
      if (baselineClicked.isEmpty())
        QVERIFY2(baselineClicked.wait(250), "Stock Qt accessible press did not click within 250 ms");
      QCOMPARE(baselineClicked.count(), 1);
    }
    baseline.hide();
    button->setFocus();
    focus = QApplication::focusWidget();
    synergy::gui::setMotionModeForTesting(synergy::gui::MotionMode::Animated);
    auto *interface = QAccessible::queryAccessibleInterface(button);
    QVERIFY(interface);
    auto *actions = interface->actionInterface();
    QVERIFY(actions);
    QVERIFY(actions->actionNames().contains(QAccessibleActionInterface::pressAction()));
    recordSignals(button, animatedTiming);
    animatedTiming.clock.start();
    trace.begin();
    actions->doAction(QAccessibleActionInterface::pressAction());
    animatedTiming.actionReturned = animatedTiming.clock.elapsed();
    {
      WaitObservation observation{"animated", animatedTiming};
      // Start capturing feedback as soon as Qt dispatches the click. Polling
      // can leave the event loop running through the first animation frames.
      if (clicked.isEmpty())
        QVERIFY2(clicked.wait(250), "Animated accessible press did not click within 250 ms");
      QCOMPARE(clicked.count(), 1);
    }
    QVERIFY(baselineTiming.clicked >= 0 && baselineTiming.clicked <= 250);
    QVERIFY(animatedTiming.clicked >= 0 && animatedTiming.clicked <= 250);
    QVERIFY2(
        animatedTiming.clicked <= baselineTiming.clicked + 60,
        "Motion adds delay beyond stock Qt accessible activation scheduling"
    );
  } else
    button->click();
  // Business action must already be routed before waiting for feedback frames.
  QCOMPARE(clicked.count(), 1);
  auto *animation = button->findChild<QVariantAnimation *>("buttonFeedbackAnimation");
  QVERIFY(animation);
  QCOMPARE(animation->duration(), 110);
  auto *overlay = button->findChild<QWidget *>("buttonFeedbackOverlay");
  QVERIFY(overlay);
  QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
  trace.record(ButtonTraceEvent::CaptureEntry);
  const auto frames = collectFrames(*button, 180);
  trace.collecting = false;
  summarizeButtonFrames(trace, frames, activation);
  saveFrames(frames, activation);
  QVERIFY2(hasIntermediate(frames), "Standard activation must paint a non-endpoint feedback frame");
  QCOMPARE(clicked.count(), 1);
  QCOMPARE(button->geometry(), geometry);
  QCOMPARE(QApplication::focusWidget(), focus);
}
void R3MotionContractTests::buttonFeedbackRendersDeterministicInteriorFrame_data()
{
  QTest::addColumn<int>("interiorMilliseconds");
  QTest::newRow("88ms") << 88;
  QTest::newRow("95ms") << 95;
}
void R3MotionContractTests::buttonFeedbackRendersDeterministicInteriorFrame()
{
  QFETCH(int, interiorMilliseconds);
  QDialog dialog;
  auto *layout = new QVBoxLayout(&dialog);
  auto *button = new QPushButton("Deterministic feedback");
  layout->addWidget(button);
  dialog.resize(260, 100);
  dialog.setAttribute(Qt::WA_DontShowOnScreen);
  dialog.show();
  button->setFocus();
  QCoreApplication::processEvents();
  const auto geometry = button->geometry();
  auto *focus = QApplication::focusWidget();
  QSignalSpy clicked(button, &QPushButton::clicked);

  button->click();
  QCOMPARE(clicked.count(), 1);
  QCOMPARE(button->findChildren<QWidget *>("buttonFeedbackOverlay", Qt::FindDirectChildrenOnly).size(), 1);
  auto *overlay = button->findChild<QWidget *>("buttonFeedbackOverlay", Qt::FindDirectChildrenOnly);
  QVERIFY(overlay);
  QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
  auto *animation = overlay->findChild<QVariantAnimation *>("buttonFeedbackAnimation", Qt::FindDirectChildrenOnly);
  QVERIFY(animation);
  QCOMPARE(animation->duration(), 110);
  QVERIFY(animation->state() == QAbstractAnimation::Running);

  animation->setCurrentTime(0);
  QCoreApplication::processEvents();
  const auto start = button->grab().toImage();
  const auto startOpacity = overlay->property("motionOpacity").toDouble();
  const auto animationStart = animation->startValue().toDouble();
  QVERIFY2(qAbs(startOpacity - animationStart) < 0.000001, "Overlay opacity must begin at its animation start value");

  animation->setCurrentTime(interiorMilliseconds);
  QCoreApplication::processEvents();
  const auto interior = button->grab().toImage();
  const auto interiorOpacity = overlay->property("motionOpacity").toDouble();
  QVERIFY2(animation->state() == QAbstractAnimation::Running, "Interior sample must be inside the running animation");

  animation->setCurrentTime(110);
  QCoreApplication::processEvents();
  const auto end = button->grab().toImage();
  qInfo().nospace() << "MD2 deterministic interiorMs=" << interiorMilliseconds << " startOpacity=" << startOpacity
                    << " interiorOpacity=" << interiorOpacity << " startDiff=" << differentFramePixels(interior, start)
                    << " endDiff=" << differentFramePixels(interior, end);
  QVERIFY2(differentFramePixels(interior, start) > 0, "Interior frame must differ from the animation start image");
  QVERIFY2(differentFramePixels(interior, end) > 0, "Interior frame must differ from the completed image");
  QCOMPARE(clicked.count(), 1);
  QCOMPARE(button->geometry(), geometry);
  QCOMPARE(QApplication::focusWidget(), focus);

  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCOMPARE(button->findChildren<QWidget *>("buttonFeedbackOverlay", Qt::FindDirectChildrenOnly).size(), 0);
}
void R3MotionContractTests::contentTransitionBoundsRenderedBrightness()
{
  QWidget surface;
  surface.setObjectName("contentBrightnessFixture");
  surface.setStyleSheet("QWidget#contentBrightnessFixture { background-color: #102030; }");
  surface.resize(240, 120);
  surface.setAttribute(Qt::WA_DontShowOnScreen);
  surface.show();
  QCoreApplication::processEvents();
  const auto geometry = surface.geometry();
  auto *focus = QApplication::focusWidget();
  const auto start = surface.grab().toImage();

  synergy::gui::animateContentTransition(&surface);
  QCOMPARE(surface.findChildren<QWidget *>("contentTransitionOverlay", Qt::FindDirectChildrenOnly).size(), 1);
  QPointer<QWidget> overlay = surface.findChild<QWidget *>("contentTransitionOverlay", Qt::FindDirectChildrenOnly);
  QVERIFY(overlay);
  QVERIFY(overlay->isVisible());
  QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
  auto *animation = overlay->findChild<QVariantAnimation *>("contentTransitionAnimation", Qt::FindDirectChildrenOnly);
  QVERIFY(animation);
  QCOMPARE(animation->duration(), 160);
  QCOMPARE(animation->state(), QAbstractAnimation::Running);
  animation->pause();
  animation->setCurrentTime(0);
  const auto firstOpacity = overlay->property("motionOpacity").toDouble();
  const auto peak = surface.grab().toImage();
  animation->setCurrentTime(80);
  QCOMPARE(animation->currentTime(), 80);
  const auto middleOpacity = overlay->property("motionOpacity").toDouble();
  const auto middle = surface.grab().toImage();
  animation->setCurrentTime(160);
  const auto end = surface.grab().toImage();

  QVERIFY(!start.isNull());
  QCOMPARE(peak.size(), start.size());
  QCOMPARE(middle.size(), start.size());
  QCOMPARE(end.size(), start.size());
  const QPoint sample(start.width() / 2, start.height() / 2);
  const auto startColor = start.pixelColor(sample);
  const auto peakColor = peak.pixelColor(sample);
  const auto middleColor = middle.pixelColor(sample);
  const auto endColor = end.pixelColor(sample);
  const std::array<int, 3> increments{
      peakColor.red() - startColor.red(), peakColor.green() - startColor.green(), peakColor.blue() - startColor.blue()
  };
  qInfo().nospace() << "CCF1 firstOpacity=" << firstOpacity << " middleOpacity=" << middleOpacity
                    << " startColor=" << startColor << " peakColor=" << peakColor << " middleColor=" << middleColor
                    << " endColor=" << endColor;
  saveFrames({start, peak, middle, end}, "content-brightness");
  for (const int increment : increments) {
    QVERIFY2(increment > 0, "Content transition must visibly brighten the owned dark surface");
    QVERIFY2(increment <= 16, "Content transition peak must add no more than 16 per 8-bit color channel");
  }
  QVERIFY2(middle != peak && middle != end, "Content transition must retain a rendered non-endpoint frame");
  QCOMPARE(end, start);
  QCOMPARE(surface.geometry(), geometry);
  QCOMPARE(QApplication::focusWidget(), focus);
  QCOMPARE(surface.findChildren<QWidget *>("contentTransitionOverlay", Qt::FindDirectChildrenOnly).size(), 0);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(overlay.isNull());
}
void R3MotionContractTests::tabAndOptionTransitionsPaint()
{
  synergy::gui::ServerConfig config;
  SettingsDialog dialog(nullptr, config);
  dialog.setAttribute(Qt::WA_DontShowOnScreen);
  dialog.show();
  QCoreApplication::processEvents();
  auto *tabs = dialog.findChild<QTabWidget *>();
  QVERIFY(tabs && tabs->count() > 1);
  tabs->setCurrentIndex(1);
  const auto frames = collectFrames(dialog);
  saveFrames(frames, "active-tab");
  QVERIFY2(hasIntermediate(frames), "Actual tab transition must interpolate rendered frames");
  QCOMPARE(tabs->currentIndex(), 1);
  tabs->setCurrentIndex(0);
  QTest::qWait(180);
  auto *options = dialog.findChild<QComboBox *>("comboTrayIcon");
  QVERIFY(options && options->count() == 2);
  const int target = options->currentIndex() == 0 ? 1 : 0;
  options->setCurrentIndex(target);
  const auto optionFrames = collectFrames(dialog);
  saveFrames(optionFrames, "option-change");
  QVERIFY2(hasIntermediate(optionFrames), "Actual option change must interpolate rendered frames");
  QCOMPARE(options->currentIndex(), target);
  dialog.reject();
}
void R3MotionContractTests::interruptionResizeBackAndDestruction()
{
  auto host = std::make_unique<PageHost>();
  auto *stack = prepareHost(*host);
  auto *parent = page("#447722");
  auto *child = page("#2255aa");
  QPointer<QDialog> parentLifetime(parent), childLifetime(child);
  host->push(parent);
  QTest::qWait(25);
  host->push(child);
  QTest::qWait(25);
  host->resize(540, 420);
  QSignalSpy childRejected(child, &QDialog::rejected);
  child->reject();
  QCOMPARE(childRejected.count(), 1);
  QTest::qWait(190);
  QVERIFY(host->hasEditor());
  QVERIFY(!parentLifetime.isNull());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(childLifetime.isNull());
  QCOMPARE(host->findChildren<QWidget *>("contentTransitionOverlay").size(), 0);
  QCOMPARE(host->width(), 540);
  QVERIFY(stack->currentIndex() != 0);
  parent->reject();
  QTest::qWait(20);
  host.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(parentLifetime.isNull());
  // Destroy an activated button while its feedback is in flight.
  auto *button = new QPushButton("Destroy while animating");
  button->setAttribute(Qt::WA_DontShowOnScreen);
  button->show();
  QPointer<QPushButton> tracked(button);
  button->click();
  delete button;
  QTest::qWait(180);
  QVERIFY(tracked.isNull());
}
void R3MotionContractTests::reducedMotionSettlesImmediately()
{
  PageHost host;
  auto *stack = prepareHost(host);
  host.push(page("#882244"));
  QTest::qWait(25);
  synergy::gui::setMotionModeForTesting(synergy::gui::MotionMode::Reduced);
  QVERIFY(synergy::gui::reducedMotion());
  QCoreApplication::processEvents();
  QCOMPARE(host.findChildren<QWidget *>("contentTransitionOverlay").size(), 0);
  auto *editor = page("#228844");
  host.push(editor);
  QCoreApplication::processEvents();
  QVERIFY(host.hasEditor());
  QCOMPARE(host.findChildren<QWidget *>("contentTransitionOverlay").size(), 0);
  const auto first = host.grab().toImage();
  QTest::qWait(50);
  QCOMPARE(host.grab().toImage(), first);
  editor->reject();
  QCoreApplication::processEvents();
  QCOMPARE(stack->count(), 2);
  QPushButton button("Immediate");
  QSignalSpy clicked(&button, &QPushButton::clicked);
  button.click();
  QCOMPARE(clicked.count(), 1);
  QCoreApplication::processEvents();
  for (auto *animation : button.findChildren<QVariantAnimation *>())
    QVERIFY(animation->state() != QAbstractAnimation::Running);
}
int main(int argc, char **argv)
{
  QTemporaryDir directory;
  if (!directory.isValid())
    return 1;
  const auto root = QFileInfo(directory.path()).canonicalFilePath();
  QString error;
  if (!synergy::runtime::configureIsolatedRuntime(
          {root, "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces)}, &error
      ))
    return 2;
  QApplication application(argc, argv);
  Settings::setSettingsFile(root + "/user/motion.conf");
  Settings::setStateFile(root + "/state/app.state");
  Settings::setValue(Settings::Core::ComputerName, "Motion-Fixture");
  Settings::setValue(Settings::Core::Language, "en");
  Settings::setValue(Settings::Gui::AutoStartCore, false);
  Settings::setValue(Settings::Gui::CheckForUpdates, false);
  application.setStyleSheet(synergy::gui::appStyleSheet());
  synergy::gui::installButtonFeedback();
  R3MotionContractTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3MotionContractTests.moc"
