#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

#include "rDebug_CLIDemo.h"


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    JobRunner* job = new JobRunner( &a );

    QObject::connect( job, SIGNAL(done()), &a , SLOT(quit()) );
    QObject::connect( job, SIGNAL(done()), job, SLOT(on_done()) );

    // perform the job from applications event loop.
    QTimer::singleShot( 10/*ms*/, job, SLOT(on_run()) );

    return a.exec();
}
