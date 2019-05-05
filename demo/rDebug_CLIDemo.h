#ifndef RDEBUG_CLIDEMO_H
#define RDEBUG_CLIDEMO_H

#include <QObject>
#include <QDebug>
#include <QTextStream>

class JobRunner : public QObject
{
Q_OBJECT

public:
    JobRunner( QObject *parent = nullptr )
      : QObject(parent)
    {}


signals:
    void done(void);

public slots:
    void on_run(void)
    {   // Do processing here
        qDebug() << __PRETTY_FUNCTION__ << "running...";


        QTextStream out(stdout);
        out << "Output to QTextStream" << endl;

        int i=8;
        do
        {
            qDebug() << "loop" << i <<"performed";
        } while( --i >=0 );

        qInfo() <<     "qInfo    : C++ Style Info Message";
        qInfo(         "qInfo    : C-  Style Info Message" );
        qDebug() <<    "qDebug   : C++ Style Debug Message";
        qDebug(        "qDebug   : C-  Style Debug Message" );
        qWarning() <<  "qWarning : C++ Style Warning Message";
        qWarning(      "qWarning : C-  Style Warning Message" );
        qCritical() << "qCritical: C++ Style Critical Error Message";
        qCritical(     "qCritical: C-  Style Critical Error Message" );
        // qFatal does not have a C++ style method.
        // qFatal will immediately close the app, so we just enable it if we want to test this special behaviour
        //qFatal(        "qFatal   : C-  Style Fatal Error Message" );
        emit done();
    }

    void on_done(void)
    {   // say goodbye
        QTextStream(stdout) << "=== Good Bye! ===" << endl;
    }


private:
};


#endif // RDEBUG_CLIDEMO_H
