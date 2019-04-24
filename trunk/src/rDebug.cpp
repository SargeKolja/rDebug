/**
 * Project "rDebug"
 *
 * rDebug.cpp
 * 
 * Loving qDebug? But missing some things? 
 * Just want to see your debugs/logs in a QtWidget, like QListView? 
 * But got crashes with qInstallMsgHandler (4.x) / qInstallMessageHandler (5.x)?
 * Missing file name, line numbers with qDebug 4.x?
 *
 * You are welcome, here we go!
 *
 * This is mostly a simplified re-implementation, which under the hood also uses parts of qDebug(). 
 * You can use
 *    qDebug( "printf mask %s","is worse");
 * or 
 *    qDebug() << "streams" << "are more safe";
 * or a mix of both. You can select between all 7 BSD-Syslog levels (plus "Silent" and "All").
 * A two-liner can send Qt SIGNAL with the data, time stamp, location and level to your QListWidget (or whatever).
 * Another two-liner can write to file and use different log level filtering. 
 * For the start, some features of Qt5, like 
 *    qDebug( &stream_device ) << "are not supported"; // and may never come.
 * 
 * copyright 2019 Sergeant Kolja, GERMANY
 * 
 * distributed under the terms of the 2-clause license also known as "Simplified BSD License" or "FreeBSD License"
 * License is compatible with GPL and LGPL
 */ 
 
#include <QtGlobal>
#include <QDebug>
#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <stdio.h>
#include <stdarg.h>  // va_list

#include "rDebugLevel.h"

void to_xDebug( rDebugLevel::rMsgType Level, const QString& msg )
{
  switch(Level)
  {
    case rDebugLevel::rMsgType::Emergency    : qFatal( msg.toLocal8Bit().constData() ); //break;
    case rDebugLevel::rMsgType::Alert        :
    case rDebugLevel::rMsgType::Critical     :
    case rDebugLevel::rMsgType::Error        : qCritical( msg.toLocal8Bit().constData() ); break;
    case rDebugLevel::rMsgType::Warning      : qWarning( msg.toLocal8Bit().constData() ); break;
    case rDebugLevel::rMsgType::Notice       :
    case rDebugLevel::rMsgType::Informational:
    case rDebugLevel::rMsgType::Debug        : qDebug().noquote() << msg; break;
    default                                  : break;
  }
}

#include "rDebug.h" // this MUST be after the implemnentation of to_xDebug, to use the overloaded Macros there


#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY 16 // 16-23 are application default values
#endif

#ifndef SYSLOG_LEVEL_MAX
#define SYSLOG_LEVEL_MAX rDebugLevel::rMsgType::Warning
#endif

#ifndef SYSLOG_WITH_NUMERIC_8DIGITS_ID
#define SYSLOG_WITH_NUMERIC_8DIGITS_ID true
#endif
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

rDebugLevel::rMsgType rDebug_GlobalLevel::mMaxLevel = SYSLOG_LEVEL_MAX;


rDebug_GlobalLevel::rDebug_GlobalLevel(rDebugLevel::rMsgType MaxLevel)
{
  rDebug_GlobalLevel::mMaxLevel = MaxLevel;
}

void rDebug_GlobalLevel::set(rDebugLevel::rMsgType MaxLevel)
{
  rDebug_GlobalLevel::mMaxLevel = MaxLevel;
}

rDebugLevel::rMsgType rDebug_GlobalLevel::get(void)
{
  return rDebug_GlobalLevel::mMaxLevel;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

rDebugLevel::rMsgType rDebug_Signaller::mMaxLevel = SYSLOG_LEVEL_MAX;
rDebug_Signaller*     rDebug_Signaller::pSignaller = nullptr;


rDebug_Signaller::rDebug_Signaller(rDebugLevel::rMsgType MaxLevel)
{
  rDebug_Signaller::mMaxLevel = MaxLevel;
  if( MaxLevel <= rDebugLevel::rMsgType::Silent )
  { rDebug_Signaller::pSignaller = nullptr;
    return;
  }
  rDebug_Signaller::pSignaller = this;
}


rDebug_Signaller::~rDebug_Signaller()
{
  rDebug_Signaller::pSignaller = nullptr;
}

void rDebug_Signaller::setMaxLevel(rDebugLevel::rMsgType MaxLevel)
{
  rDebug_Signaller::mMaxLevel = MaxLevel;
}

void rDebug_Signaller::signal_line( const FileLineFunc_t& CodeLocation, const QDateTime& Time, rDebugLevel::rMsgType Level, uint64_t LogId, const QString& line )
{
  if( rDebug_Signaller::mMaxLevel >= Level)
  {
    int lvl = static_cast<int>(Level);
    emit sig_logline( CodeLocation, Time, lvl, LogId, line );
  }
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

rDebugLevel::rMsgType rDebug_Filewriter::mMaxLevel = SYSLOG_LEVEL_MAX;
bool                  rDebug_Filewriter::mDumpCodeLocation = false;
rDebug_Filewriter*    rDebug_Filewriter::pFilewriter = nullptr;

rDebug_Filewriter::rDebug_Filewriter(const QString& fileName, rDebugLevel::rMsgType MaxLevel, qint16 MaxBackups, qint64 MaxSize)
  : mFileName(fileName)
  , mMaxSize( qMax( MaxSize, static_cast<qint64>(0x10000) ) )
  , mMaxBackups(MaxBackups)
  , mpLogfile(nullptr)
{
  rDebug_Filewriter::mMaxLevel = MaxLevel;
  if( MaxLevel <= rDebugLevel::rMsgType::Silent )
  { rDebug_Filewriter::pFilewriter = nullptr;
    return;
  }

  rDebug_Filewriter::pFilewriter = this;

  if( mFileName.isEmpty() )
  { mFileName = QDir::tempPath() + '/' + QFileInfo( QCoreApplication::applicationFilePath() ).fileName() + ".log";
  }

  if (!mFileName.isEmpty())
  {
      if( oversized(mFileName) )
      { rotate();
      }
      open( mFileName, "CTor", "========== logfile opened ==========" );
  }
}


rDebug_Filewriter::~rDebug_Filewriter()
{
  close( "DTor", "========== logfile closed ==========" );
  rDebug_Filewriter::pFilewriter = nullptr;
}


void rDebug_Filewriter::setMaxLevel(rDebugLevel::rMsgType MaxLevel)
{
  rDebug_Filewriter::mMaxLevel = MaxLevel;
}


void rDebug_Filewriter::setMaxSize(qint64 MaxSize)
{
  mMaxSize = MaxSize;
}


void rDebug_Filewriter::setMaxBackups( qint16 MaxBackups )
{
  mMaxBackups = MaxBackups;
}


void rDebug_Filewriter::enableCodeLocations( bool enable )
{
    rDebug_Filewriter::mDumpCodeLocation = enable;
}



void rDebug_Filewriter::write_file(const FileLineFunc_t& CodeLocation, const QDateTime& Time, rDebugLevel::rMsgType Level, uint64_t LogId, const QString& line)
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  if( rDebug_Filewriter::mMaxLevel < Level )
    return;

  rotate_ondemand();

  write_file_raw( CodeLocation, Time, Level, LogId, line );
#endif //!defined( QT_NO_DEBUG_OUTPUT )
}


void rDebug_Filewriter::write_file_raw(const FileLineFunc_t& CodeLocation, const QDateTime& Time, rDebugLevel::rMsgType Level, uint64_t LogId, const QString& line)
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  QTextStream WholeMsg( mpLogfile );
  WholeMsg << QString("%1 [%2] %3, %4") \
                  .arg( rDebugBase::getDateTimeStr( Time ) ) \
                  .arg( rDebugBase::getLevelName( Level ) ) \
                  .arg( rDebugBase::getLogIdStr( LogId, 0 ) ) \
                  .arg( line ) \
                  ;
  if( rDebug_Filewriter::mDumpCodeLocation )
  {
    WholeMsg << QString(" {from %1 in %2:%3}") \
                  .arg( CodeLocation.mFunc ? CodeLocation.mFunc : "func" ) \
                  .arg( CodeLocation.mFile ? CodeLocation.mFile : "file" ) \
                  .arg( CodeLocation.mLine ) \
                  ;
  }
  WholeMsg << QString("\n");
  WholeMsg.flush();
#endif //!defined( QT_NO_DEBUG_OUTPUT )
}


void rDebug_Filewriter::write_wrap(const char* Location, const char* Reason)
{
  FileLineFunc_t here(__FILE__, __LINE__, Location);
  QDateTime Now( QDate::currentDate(), QTime::currentTime(), Qt::LocalTime );
  write_file_raw( here, Now, rDebugLevel::rMsgType::Notice, 0, Reason );
}


void rDebug_Filewriter::open(const QString& fileName, const char* Location, const char* Reason)
{
  mpLogfile = new QFile(fileName);
  mpLogfile->open(QIODevice::Append | QIODevice::Text);
  write_wrap( Location, Reason );
}


void rDebug_Filewriter::close(const char* Location, const char* Reason)
{
  write_wrap( Location, Reason );
  mpLogfile->close();
  mpLogfile = nullptr;
}


void rDebug_Filewriter::rotate(void)
{
  const int32_t MinBackupIndex = 1;
  QFileInfo fi( mFileName );
  int32_t Index = MinBackupIndex;
  for( ; Index<mMaxBackups ; ++Index )
  {
    QFileInfo probe( fi.path() + '/' + fi.completeBaseName() + QString(".%1.").arg(Index) + fi.suffix() );
    if( ! probe.exists() )
      break;
  }
  // Index is last used backup + 1 now
  for( ; Index>(MinBackupIndex-1) ; --Index )
  {
    QString free_file( fi.path() + '/' + fi.completeBaseName() + QString(".%1.").arg(Index  ) + fi.suffix() );
    QString used_file( fi.path() + '/' + fi.completeBaseName() + QString(".%1.").arg(Index-1) + fi.suffix() );
    if( Index==MinBackupIndex )
      used_file = mFileName;

    QFile( used_file ).rename( free_file );
  }
}


void rDebug_Filewriter::rotate_ondemand(void)
{
  if( oversized( *mpLogfile ) )
  {
    bool isopen = ( mpLogfile && mpLogfile->isOpen() ) ? true : false;
    if( isopen )
    {
      const char *Who = "Rotator";
      const char *Reason = "~~~~~~~~~~ logfile rotated ~~~~~~~~~~";
      close( Who, Reason );

      rotate();

      open( mFileName, Who, Reason );
    }
    else
    {
      rotate();
    }
  }
}


bool rDebug_Filewriter::oversized( const QString& fileName )
{
  QFileInfo fi( fileName );
  if( fi.exists() && fi.size() > (mMaxSize - 128) ) // 128 bytes reserve to enshure the "closed/rolled" entry fits also
      return true;
  return false;
}


bool rDebug_Filewriter::oversized( QFile& openedFile )
{
  QFileInfo fi( openedFile );
  if( fi.isReadable() && fi.size() > (mMaxSize - 128) ) // 128 bytes reserve to enshure the "closed/rolled" entry fits also
      return true;
  return false;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


rDebugBase::rDebugBase(const char *file, int line, const char* func, rDebugLevel::rMsgType Level, uint64_t LogId)
  : mBase(10)
  , mFileLineFunc(file,line,func)
  , mLevel(Level)
  , mFacility(SYSLOG_FACILITY)
  , mTime( QDate::currentDate(), QTime::currentTime(), Qt::LocalTime )
  , mLogId(LogId)
  , mMsgBuffer("")
  , mMsgStream(&mMsgBuffer)
  , mWithLogId(SYSLOG_WITH_NUMERIC_8DIGITS_ID)
{}


rDebugBase::~rDebugBase()
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  output( mLevel );
#endif //!defined( QT_NO_DEBUG_OUTPUT )
}



void rDebugBase::output( rDebugLevel::rMsgType currLevel )
{
  QSignalBackendWriter( currLevel );
  QDebugBackendWriter( currLevel );
  QFileBackendWriter( currLevel );
}

void rDebugBase::QDebugBackendWriter( rDebugLevel::rMsgType currLevel )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  if( rDebug_GlobalLevel::get() < currLevel )
    return;

  QString     mStringDevice("");
  QTextStream WholeMsg( &mStringDevice );
  if( mWithLogId )
      WholeMsg << QString("%1 [%2] %3, %4") \
                  .arg( getDateTimeStr( mTime ) ) \
                  .arg( getLevelName( mLevel ) ) \
                  .arg( getLogIdStr( mLogId ) ) \
                  .arg( mMsgBuffer ) ;
  else
    WholeMsg << QString("%1 [%2] %4") \
                  .arg( getDateTimeStr( mTime ) ) \
                  .arg( getLevelName( mLevel ) ) \
                  .arg( mMsgBuffer ) ;

  WholeMsg.flush();

  to_xDebug( mLevel, mStringDevice );
#endif //!defined( QT_NO_DEBUG_OUTPUT )
}


void rDebugBase::QSignalBackendWriter( rDebugLevel::rMsgType currLevel )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  if( rDebug_GlobalLevel::get() < currLevel )
    return;

  if( rDebug_Signaller::pSignaller )
  {   rDebug_Signaller::pSignaller->signal_line( mFileLineFunc, mTime, mLevel, mLogId, mMsgBuffer );
  }
#endif //!defined( QT_NO_DEBUG_OUTPUT )
}


void rDebugBase::QFileBackendWriter( rDebugLevel::rMsgType currLevel )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  if( rDebug_GlobalLevel::get() < currLevel )
    return;

  if( rDebug_Filewriter::pFilewriter )
  {   rDebug_Filewriter::pFilewriter->write_file( mFileLineFunc, mTime, mLevel, mLogId, mMsgBuffer );
  }
#endif //!defined( QT_NO_DEBUG_OUTPUT )
}


void rDebugBase::writer( rDebugLevel::rMsgType Level, uint64_t LogId, bool withLogId, const char* msg, va_list valist )
{
  mWithLogId = withLogId;
  mLogId     = (LogId) ? LogId : static_cast<uint64_t>(QCoreApplication::applicationPid());
  mLevel     = Level;

  if( rDebug_GlobalLevel::get() < Level )
    return;

  if(!msg)
    return;

  size_t allocated = 1024;
  char* str = new char[allocated];
  *str=0;
  int attempted;
  attempted = vsnprintf( str, allocated, msg, valist );
  if( attempted >= static_cast<int>(allocated) )
  {
    delete [] str;
    allocated = static_cast<size_t>(attempted + 1);
    str = new char[ allocated ];
    vsnprintf( str, allocated, msg, valist );
  }

  mMsgBuffer.append(str);
  delete [] str;
}




QString rDebugBase::getLevelName( rDebugLevel::rMsgType Level )
{
    QString LevelName;

    switch( Level )
    {
        case rDebugLevel::rMsgType::Debug        : LevelName = QObject::tr("Debg","[Debg], Debug-level messages Messages that contain information normally of use only when debugging a program."); break;
        case rDebugLevel::rMsgType::Informational: LevelName = QObject::tr("Info","[Info], message type of unspecified messages"); break;
        case rDebugLevel::rMsgType::Notice       : LevelName = QObject::tr("Note","[Note], Normal but significant conditions. Conditions that are not error conditions, but that may require special handling."); break;
        case rDebugLevel::rMsgType::Warning      : LevelName = QObject::tr("Warn","[Warn], Warning conditions"); break;
        case rDebugLevel::rMsgType::Error        : LevelName = QObject::tr("Err!","[Err!], Error conditions"); break;
        case rDebugLevel::rMsgType::Critical     : LevelName = QObject::tr("Crit","[Crit], Critical conditions, like hard device errors."); break;
        case rDebugLevel::rMsgType::Alert        : LevelName = QObject::tr("Alrt","[Alrt], Action must be taken immediately, A condition that should be corrected immediately, such as a corrupted system database."); break;
        case rDebugLevel::rMsgType::Emergency    : // fall through
        default                                  : LevelName = QObject::tr("Emrg","[Emrg], System is unusable. A panic condition."); break;
    }
    return LevelName;
}



QString rDebugBase::getDateTimeStr(const QDateTime& Time)
{
  QString TimestampAsString( QString("%1").arg( Time.toString(QObject::tr("yyyy-MM-dd HH:mm:ss,zzz","local date time format for logging"))) );
  return TimestampAsString;
}

QString rDebugBase::getLogIdStr(uint64_t LogId, int FormatLen)
{
  if( FormatLen == 8 ) // typical 8-digit-fixed size logid
    return QString("%1").arg( LogId, 8, 10, QLatin1Char('0') );
  else if( FormatLen == 16 )
    return QString("0x%1:%2").arg( LogId>>32, 8, 16, QLatin1Char('0') ).arg( LogId&0xFFFFFFFF, 8, 16, QLatin1Char('0') );
  else
    return QString("%1").arg( LogId );
}


rDebugBase& rDebugBase::integerBase( int base )
{
  this->mBase = base;
  return *this;
}


rDebugBase& rDebugBase::operator<<( QChar ch )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << ch;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( bool flg )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << ((flg) ? "true" : "false");
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( char ch )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << ch;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( signed short num )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << num;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( unsigned short num )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << num;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( signed int num )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << num;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( unsigned int num )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << num;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( signed long lnum )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << lnum;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( unsigned long lnum )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << lnum;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( qint64 i64 )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << i64;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( quint64 i64 )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream.setIntegerBase(mBase);
  mMsgStream << i64;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( float flt )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << flt;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( double dbl )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << dbl;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( const char * ptr )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << ((ptr)?ptr:"(nullptr)");
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( const QString & str )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << str;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( const QStringRef & str )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << str;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( const QLatin1String & str )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << str;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( const QByteArray & ba )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << ba;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::operator<<( const void * vptr )
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << "0x" << hex << vptr;
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}

rDebugBase& rDebugBase::operator<<(const QTextStream& qts)
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << qts.string();
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}

rDebugBase&rDebugBase::operator<<(const QPoint& d)
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << "@(" << d.x() << "," << d.y() << ")";
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase&rDebugBase::operator<<(const QSize& d)
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << "@(" << d.width() << "x" << d.height() << ")";
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase&rDebugBase::operator<<(const QRect& d)
{
#if !defined( QT_NO_DEBUG_OUTPUT )
  mMsgStream << "@((" << d.width() << "," << d.height() << ")+(" << d.x() << "," << d.y() << "))";
#endif //!defined( QT_NO_DEBUG_OUTPUT )
  return *this;
}


rDebugBase& rDebugBase::debug(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Debug, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::info(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Informational, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::note(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Notice, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::warning(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Warning, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::error(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Error, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::critical(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Critical, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::emergency(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Alert, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::fatal(uint64_t LogId, const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Emergency, LogId, true, msg, args );
  va_end(args);
  return (*this);
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

rDebugBase& rDebugBase::debug(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Debug, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::info(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Informational, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::note(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Notice, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::warning(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Warning, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::error(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Error, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::critical(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Critical, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::emergency(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Alert, 0, false, msg, args );
  va_end(args);
  return (*this);
}

rDebugBase& rDebugBase::fatal(const char* msg, ... )
{
  va_list args;
  va_start(args, msg);
  writer( rDebugLevel::rMsgType::Emergency, 0, false, msg, args );
  va_end(args);
  return (*this);
}


/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

rDebugBase& operator<<( rDebugBase& s, rDebugBaseManipulator f )
{ return (*f)(s); }

rDebugBase& hex( rDebugBase& s )
{
  return s.integerBase(16);
}

rDebugBase& dec( rDebugBase& s )
{
  return s.integerBase(10);
}

rDebugBase& oct( rDebugBase& s )
{
  return s.integerBase(8);
}

rDebugBase& bin( rDebugBase& s )
{
  return s.integerBase(2);
}

