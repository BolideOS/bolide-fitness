#ifndef WORKOUTCONTROLLER_H
#define WORKOUTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QDateTime>
#include <QVector>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QQmlEngine>
#include <QJSEngine>

class MGConfItem;

class WorkoutController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool workoutActive READ workoutActive NOTIFY workoutActiveChanged)
    Q_PROPERTY(QString workoutType READ workoutType NOTIFY workoutTypeChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY elapsedSecondsChanged)
    Q_PROPERTY(int heartRate READ heartRate NOTIFY heartRateChanged)
    Q_PROPERTY(QString activeProfileId READ activeProfileId NOTIFY activeProfileIdChanged)
    Q_PROPERTY(QVariantList hrHistory READ hrHistory NOTIFY hrHistoryChanged)

public:
    static QObject *qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine);
    explicit WorkoutController(QObject *parent = nullptr);
    ~WorkoutController();

    // Property getters
    bool workoutActive() const { return m_workoutActive; }
    QString workoutType() const { return m_workoutType; }
    int elapsedSeconds() const { return m_elapsedSeconds; }
    int heartRate() const { return m_heartRate; }
    QString activeProfileId() const { return m_activeProfileId; }
    QVariantList hrHistory() const { return m_hrHistory; }

    // QML invokable methods
    Q_INVOKABLE bool startWorkout(const QString &type);
    Q_INVOKABLE bool stopWorkout();
    Q_INVOKABLE QVariantList workoutTypes();
    Q_INVOKABLE QString profileForWorkout(const QString &workoutType);
    Q_INVOKABLE bool setProfileForWorkout(const QString &workoutType, const QString &profileId);
    Q_INVOKABLE QVariantList availableProfiles();

signals:
    void workoutActiveChanged();
    void workoutTypeChanged();
    void elapsedSecondsChanged();
    void heartRateChanged();
    void activeProfileIdChanged();
    void hrHistoryChanged();

private slots:
    void onElapsedTick();
    void onHRTick();
    void onPowerdActiveProfileChanged(const QString &id, const QString &name);

private:
    bool callPowerd(const QString &method, const QVariantList &args = QVariantList());
    QVariant callPowerdSync(const QString &method, const QVariantList &args = QVariantList());
    void initializeSensor();
    void updateHeartRate();
    void addHRReading(int bpm);
    void publishGlance();
    void clearGlance();

    // D-Bus interfaces
    QDBusInterface *m_powerd;
    QDBusConnection m_systemBus;

    // Timers
    QTimer *m_elapsedTimer;
    QTimer *m_hrTimer;

    // State
    bool m_workoutActive;
    QString m_workoutType;
    int m_elapsedSeconds;
    int m_heartRate;
    QString m_activeProfileId;
    QString m_previousProfileId;
    QVariantList m_hrHistory;  // List of {timestamp, bpm} maps

    // Glance data (published to dconf for launcher widget cards)
    MGConfItem *m_glanceTitle;
    MGConfItem *m_glanceValue;
    MGConfItem *m_glanceSubtitle;
    MGConfItem *m_glanceGraph;
    MGConfItem *m_glanceColors;
    MGConfItem *m_glanceTimestamp;

    // Workout type definitions
    struct WorkoutTypeDef {
        QString id;
        QString name;
        QString icon;
    };
    static const QVector<WorkoutTypeDef> s_workoutTypes;
};

#endif // WORKOUTCONTROLLER_H
