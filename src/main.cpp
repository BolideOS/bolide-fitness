#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>
#include <QtQml>

#include <asteroidapp.h>

#include "workoutcontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(AsteroidApp::application(argc, argv));
    QScopedPointer<QQuickView> view(AsteroidApp::createView());
    qmlRegisterSingletonType<WorkoutController>(
        "org.bolide.fitness", 1, 0, "WorkoutController",
        WorkoutController::qmlInstance);
    view->setSource(QUrl("qrc:/qml/main.qml"));
    view->resize(app->primaryScreen()->size());
    view->show();
    return app->exec();
}
