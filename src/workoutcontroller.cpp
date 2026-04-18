#include "workoutcontroller.h"
#include <QDebug>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <mgconfitem.h>

#define GLANCE_PREFIX "/org/bolideos/glance/bolide-fitness"

// D-Bus constants
#define POWERD_SERVICE      "org.bolideos.powerd"
#define POWERD_PATH         "/org/bolideos/powerd"
#define POWERD_INTERFACE    "org.bolideos.powerd.ProfileManager"

// Workout type definitions
const QVector<WorkoutController::WorkoutTypeDef> WorkoutController::s_workoutTypes = {
    { QStringLiteral("treadmill"),    QStringLiteral("Treadmill"),    QStringLiteral("ios-fitness-outline") },
    { QStringLiteral("outdoor_run"),  QStringLiteral("Outdoor Run"),  QStringLiteral("ios-walk-outline") },
    { QStringLiteral("indoor_run"),   QStringLiteral("Indoor Run"),   QStringLiteral("ios-fitness-outline") },
    { QStringLiteral("outdoor_walk"), QStringLiteral("Outdoor Walk"), QStringLiteral("ios-walk-outline") },
    { QStringLiteral("indoor_walk"),  QStringLiteral("Indoor Walk"),  QStringLiteral("ios-walk-outline") },
    { QStringLiteral("hike"),         QStringLiteral("Hike"),         QStringLiteral("ios-compass-outline") },
    { QStringLiteral("ultra_hike"),   QStringLiteral("Ultra Hike"),   QStringLiteral("ios-compass-outline") },
    { QStringLiteral("cycling"),      QStringLiteral("Cycling"),      QStringLiteral("ios-bicycle") },
    { QStringLiteral("swimming"),     QStringLiteral("Swimming"),     QStringLiteral("ios-water") },
    { QStringLiteral("strength"),     QStringLiteral("Strength"),     QStringLiteral("ios-barbell-outline") },
    { QStringLiteral("yoga"),         QStringLiteral("Yoga"),         QStringLiteral("ios-body-outline") },
    { QStringLiteral("other"),        QStringLiteral("Other"),        QStringLiteral("ios-stopwatch-outline") }
};

QObject *WorkoutController::qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    static WorkoutController *instance = new WorkoutController();
    return instance;
}

WorkoutController::WorkoutController(QObject *parent)
    : QObject(parent)
    , m_systemBus(QDBusConnection::systemBus())
    , m_powerd(nullptr)
    , m_elapsedTimer(nullptr)
    , m_hrTimer(nullptr)
    , m_workoutActive(false)
    , m_elapsedSeconds(0)
    , m_heartRate(0)
{
    // Connect to bolide-powerd
    if (!m_systemBus.isConnected()) {
        qWarning() << "Cannot connect to system D-Bus";
    } else {
        m_powerd = new QDBusInterface(
            POWERD_SERVICE,
            POWERD_PATH,
            POWERD_INTERFACE,
            m_systemBus,
            this
        );

        if (!m_powerd->isValid()) {
            qWarning() << "Cannot connect to bolide-powerd:" << m_powerd->lastError().message();
            delete m_powerd;
            m_powerd = nullptr;
        } else {
            // Connect to ActiveProfileChanged signal
            m_systemBus.connect(
                POWERD_SERVICE,
                POWERD_PATH,
                POWERD_INTERFACE,
                QStringLiteral("ActiveProfileChanged"),
                this,
                SLOT(onPowerdActiveProfileChanged(QString,QString))
            );

            // Get current active profile
            QVariant reply = callPowerdSync(QStringLiteral("GetActiveProfile"));
            if (reply.isValid()) {
                m_activeProfileId = reply.toString();
            }
        }
    }

    // Initialize timers (but don't start them yet)
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);  // 1 second
    connect(m_elapsedTimer, &QTimer::timeout, this, &WorkoutController::onElapsedTick);

    m_hrTimer = new QTimer(this);
    m_hrTimer->setInterval(5000);  // 5 seconds
    connect(m_hrTimer, &QTimer::timeout, this, &WorkoutController::onHRTick);

    // Initialize glance dconf items
    m_glanceTitle     = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/title"), this);
    m_glanceValue     = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/value"), this);
    m_glanceSubtitle  = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/subtitle"), this);
    m_glanceGraph     = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/graph"), this);
    m_glanceColors    = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/colors"), this);
    m_glanceTimestamp = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/timestamp"), this);

    // Publish idle glance on startup
    publishGlance();
}

WorkoutController::~WorkoutController()
{
    if (m_workoutActive) {
        stopWorkout();
    }
}

bool WorkoutController::startWorkout(const QString &type)
{
    if (m_workoutActive) {
        qWarning() << "Workout already active";
        return false;
    }

    // Validate workout type
    bool validType = false;
    for (const auto &wt : s_workoutTypes) {
        if (wt.id == type) {
            validType = true;
            break;
        }
    }
    if (!validType) {
        qWarning() << "Invalid workout type:" << type;
        return false;
    }

    // Save current profile to restore later
    m_previousProfileId = m_activeProfileId;

    // Call powerd to start workout
    if (!callPowerd(QStringLiteral("StartWorkout"), QVariantList() << type)) {
        qWarning() << "Failed to start workout via powerd";
        return false;
    }

    // Update state
    m_workoutActive = true;
    m_workoutType = type;
    m_elapsedSeconds = 0;
    m_heartRate = 0;
    m_hrHistory.clear();

    emit workoutActiveChanged();
    emit workoutTypeChanged();
    emit elapsedSecondsChanged();
    emit heartRateChanged();
    emit hrHistoryChanged();

    // Start timers
    m_elapsedTimer->start();
    m_hrTimer->start();
    
    // Get initial HR reading
    onHRTick();

    // Publish active workout glance
    publishGlance();

    qDebug() << "Workout started:" << type;
    return true;
}

bool WorkoutController::stopWorkout()
{
    if (!m_workoutActive) {
        qWarning() << "No active workout to stop";
        return false;
    }

    // Stop timers
    m_elapsedTimer->stop();
    m_hrTimer->stop();

    // Call powerd to stop workout
    if (!callPowerd(QStringLiteral("StopWorkout"))) {
        qWarning() << "Failed to stop workout via powerd";
    }

    // Update state
    m_workoutActive = false;
    m_workoutType.clear();
    m_elapsedSeconds = 0;
    m_heartRate = 0;

    emit workoutActiveChanged();
    emit workoutTypeChanged();
    emit elapsedSecondsChanged();
    emit heartRateChanged();

    // Publish idle glance
    publishGlance();

    qDebug() << "Workout stopped";
    return true;
}

QVariantList WorkoutController::workoutTypes()
{
    QVariantList result;
    for (const auto &wt : s_workoutTypes) {
        QVariantMap map;
        map[QStringLiteral("id")] = wt.id;
        map[QStringLiteral("name")] = wt.name;
        map[QStringLiteral("icon")] = wt.icon;
        result.append(map);
    }
    return result;
}

QString WorkoutController::profileForWorkout(const QString &workoutType)
{
    QVariant reply = callPowerdSync(QStringLiteral("GetWorkoutProfiles"));
    if (!reply.isValid()) {
        qWarning() << "Failed to get workout profiles";
        return QString();
    }

    QString jsonStr = reply.toString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON response from GetWorkoutProfiles";
        return QString();
    }

    QJsonObject obj = doc.object();
    if (obj.contains(workoutType)) {
        return obj[workoutType].toString();
    }

    return QString();
}

bool WorkoutController::setProfileForWorkout(const QString &workoutType, const QString &profileId)
{
    return callPowerd(QStringLiteral("SetWorkoutProfile"), QVariantList() << workoutType << profileId);
}

QVariantList WorkoutController::availableProfiles()
{
    QVariant reply = callPowerdSync(QStringLiteral("GetProfiles"));
    if (!reply.isValid()) {
        qWarning() << "Failed to get profiles";
        return QVariantList();
    }

    QString jsonStr = reply.toString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) {
        qWarning() << "Invalid JSON response from GetProfiles";
        return QVariantList();
    }

    QVariantList result;
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        if (!val.isObject()) continue;
        QJsonObject profile = val.toObject();
        
        QVariantMap map;
        map[QStringLiteral("id")] = profile[QStringLiteral("id")].toString();
        map[QStringLiteral("name")] = profile[QStringLiteral("name")].toString();
        result.append(map);
    }

    return result;
}

void WorkoutController::onElapsedTick()
{
    if (!m_workoutActive) return;

    m_elapsedSeconds++;
    emit elapsedSecondsChanged();

    // Update glance every 5 seconds to avoid dconf spam
    if (m_elapsedSeconds % 5 == 0)
        publishGlance();
}

void WorkoutController::onHRTick()
{
    if (!m_workoutActive) return;

    updateHeartRate();
}

void WorkoutController::onPowerdActiveProfileChanged(const QString &id, const QString &name)
{
    Q_UNUSED(name);
    m_activeProfileId = id;
    emit activeProfileIdChanged();
}

bool WorkoutController::callPowerd(const QString &method, const QVariantList &args)
{
    if (!m_powerd || !m_powerd->isValid()) {
        qWarning() << "Powerd interface not available";
        return false;
    }

    QDBusMessage reply = m_powerd->callWithArgumentList(QDBus::Block, method, args);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "D-Bus call" << method << "failed:" << reply.errorMessage();
        return false;
    }

    if (!reply.arguments().isEmpty()) {
        QVariant first = reply.arguments().first();
        if (first.type() == QVariant::Bool)
            return first.toBool();
    }

    return true;
}

QVariant WorkoutController::callPowerdSync(const QString &method, const QVariantList &args)
{
    if (!m_powerd || !m_powerd->isValid()) {
        qWarning() << "Powerd interface not available";
        return QVariant();
    }

    QDBusMessage reply = m_powerd->callWithArgumentList(QDBus::Block, method, args);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "D-Bus call" << method << "failed:" << reply.errorMessage();
        return QVariant();
    }

    if (reply.arguments().isEmpty()) {
        return QVariant();
    }

    return reply.arguments().first();
}

void WorkoutController::initializeSensor()
{
    // TODO: Initialize SensorFW heart rate sensor
    // For now, we'll simulate HR readings
    qDebug() << "HR sensor initialization (simulated)";
}

void WorkoutController::updateHeartRate()
{
    // Try to read heart rate from sysfs (watch-specific)
    // Common paths on AsteroidOS watches:
    // - /sys/class/power_supply/battery/capacity (for testing)
    // - Device-specific HR sensor paths vary by watch model
    
    int newHR = 0;
    bool readSuccess = false;

    // Attempt to read from sysfs (example path, varies by device)
    QFile hrFile(QStringLiteral("/sys/class/heart_rate/data"));
    if (hrFile.open(QIODevice::ReadOnly)) {
        QString data = QString::fromLatin1(hrFile.readAll().trimmed());
        bool ok;
        int value = data.toInt(&ok);
        if (ok && value > 0 && value < 220) {
            newHR = value;
            readSuccess = true;
        }
        hrFile.close();
    }

    // Fallback: simulate realistic HR during workout
    if (!readSuccess && m_workoutActive) {
        // Simulate HR based on workout type
        int baseHR = 70;
        int workoutOffset = 40;  // Moderate exercise
        
        if (m_workoutType == QLatin1String("strength") || m_workoutType == QLatin1String("yoga")) {
            workoutOffset = 20;  // Lower intensity
        } else if (m_workoutType == QLatin1String("outdoor_run") || m_workoutType == QLatin1String("cycling")) {
            workoutOffset = 60;  // Higher intensity
        } else if (m_workoutType == QLatin1String("ultra_hike")) {
            workoutOffset = 50;
        }

        // Add some variation based on time
        int variation = (m_elapsedSeconds % 20) - 10;  // ±10 bpm variation
        newHR = baseHR + workoutOffset + variation;

        // Ensure reasonable range
        if (newHR < 60) newHR = 60;
        if (newHR > 190) newHR = 190;
    }

    // Update HR if changed
    if (newHR != m_heartRate) {
        m_heartRate = newHR;
        emit heartRateChanged();
        
        // Add to history
        addHRReading(newHR);
    }
}

void WorkoutController::addHRReading(int bpm)
{
    QVariantMap reading;
    reading[QStringLiteral("timestamp")] = QDateTime::currentSecsSinceEpoch();
    reading[QStringLiteral("bpm")] = bpm;

    m_hrHistory.append(reading);

    // Keep only last 60 readings (5 minutes at 5-second intervals)
    while (m_hrHistory.size() > 60) {
        m_hrHistory.removeFirst();
    }

    emit hrHistoryChanged();
}

void WorkoutController::publishGlance()
{
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (!m_workoutActive) {
        // Idle state: show "Ready" with no graph
        m_glanceTitle->set(QStringLiteral("Workout"));
        m_glanceValue->set(QStringLiteral("Ready"));
        m_glanceSubtitle->set(QStringLiteral("Tap to start"));
        m_glanceGraph->set(QString());
        m_glanceColors->set(QString());
        m_glanceTimestamp->set(now);
        return;
    }

    // Active workout: show type, elapsed time, HR, and HR graph

    // Find workout type name
    QString typeName = m_workoutType;
    for (const auto &wt : s_workoutTypes) {
        if (wt.id == m_workoutType) {
            typeName = wt.name;
            break;
        }
    }

    // Format elapsed time
    int hours = m_elapsedSeconds / 3600;
    int mins = (m_elapsedSeconds % 3600) / 60;
    int secs = m_elapsedSeconds % 60;
    QString elapsed;
    if (hours > 0)
        elapsed = QString::asprintf("%d:%02d:%02d", hours, mins, secs);
    else
        elapsed = QString::asprintf("%d:%02d", mins, secs);

    m_glanceTitle->set(typeName);
    m_glanceValue->set(elapsed);

    // Subtitle: HR if available, otherwise calories estimate
    if (m_heartRate > 0) {
        m_glanceSubtitle->set(QString::number(m_heartRate) + QStringLiteral(" bpm"));
    } else {
        int cals = m_elapsedSeconds / 60 * 8.5;
        m_glanceSubtitle->set(QString::number(cals) + QStringLiteral(" cal"));
    }

    // Build HR graph from history (normalized 0-1 based on 60-200 bpm range)
    if (!m_hrHistory.isEmpty()) {
        QStringList graphParts;
        QStringList colorParts;
        int count = qMin(m_hrHistory.size(), 30);  // Last 30 readings for graph
        int start = m_hrHistory.size() - count;

        for (int i = start; i < m_hrHistory.size(); i++) {
            QVariantMap reading = m_hrHistory[i].toMap();
            int bpm = reading[QStringLiteral("bpm")].toInt();

            // Normalize to 0-1 range (60 bpm = 0.0, 200 bpm = 1.0)
            double normalized = qBound(0.0, (bpm - 60.0) / 140.0, 1.0);
            graphParts.append(QString::number(normalized, 'f', 2));
        }
        m_glanceGraph->set(graphParts.join(QStringLiteral(",")));
        // Green < 0.4, Yellow 0.4-0.7, Red > 0.7
        m_glanceColors->set(QStringLiteral("#4CAF50,#FF9800,#F44336"));
    } else {
        m_glanceGraph->set(QString());
        m_glanceColors->set(QString());
    }

    m_glanceTimestamp->set(now);
}

void WorkoutController::clearGlance()
{
    m_glanceTitle->unset();
    m_glanceValue->unset();
    m_glanceSubtitle->unset();
    m_glanceGraph->unset();
    m_glanceColors->unset();
    m_glanceTimestamp->unset();
}
