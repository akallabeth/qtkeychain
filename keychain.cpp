/******************************************************************************
 *   Copyright (C) 2011-2015 Frank Osterfeld <frank.osterfeld@gmail.com>      *
 *                                                                            *
 * This program is distributed in the hope that it will be useful, but        *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. For licensing and distribution        *
 * details, check the accompanying file 'COPYING'.                            *
 *****************************************************************************/
#include "keychain.h"
#include "keychain_p.h"

using namespace QKeychain;

Job::Job( const QString& service, JobPrivate *priv, QObject *parent )
    : QObject( parent )
    , d ( priv ) {
}

void Job::start() {
    JobExecutor::instance()->enqueue( this );
}

Job::~Job() {
    delete d;
}

QString Job::service() const {
    return d->service;
}

QSettings* Job::settings() const {
    return d->settings;
}

void Job::setSettings( QSettings* settings ) {
    d->settings = settings;
}

void Job::execute() {
    start();

    // Wait for job to be processed...
    QMutexLocker ml(&m_mux);
    m_finished.wait(&m_mux);
}

bool Job::autoDelete() const {
    return d->autoDelete;
}

void Job::setAutoDelete( bool autoDelete ) {
    d->autoDelete = autoDelete;
}

bool Job::insecureFallback() const {
    return d->insecureFallback;
}

void Job::setInsecureFallback( bool insecureFallback ) {
    d->insecureFallback = insecureFallback;
}

void Job::emitFinished() {
    emit finished( this );
    QMutexLocker ml(&m_mux);
    m_finished.wakeAll();

    if ( d->autoDelete )
        deleteLater();
}

void Job::emitFinishedWithError( Error error, const QString& errorString ) {
    d->error = error;
    d->errorString = errorString;
    emitFinished();
}

QString Job::key() const {
    return d->key;
}

void Job::setKey(const QString &value) {
    d->key = value;
}

Error Job::error() const {
    return d->error;
}

QString Job::errorString() const {
    return d->errorString;
}

void Job::setError( Error error ) {
    d->error = error;
}

void Job::setErrorString( const QString& errorString ) {
    d->errorString = errorString;
}

ReadPasswordJob::ReadPasswordJob( const QString& service, QObject* parent )
    : Job( service, new ReadPasswordJobPrivate( service, this ), parent )
{}

ReadPasswordJob::~ReadPasswordJob() {
}

QString ReadPasswordJob::textData() const {
    return QString::fromUtf8( d->data );
}

QByteArray ReadPasswordJob::binaryData() const {
    return d->data;
}

WritePasswordJob::WritePasswordJob( const QString& service, QObject* parent )
    : Job( service, new WritePasswordJobPrivate( service, this ), parent ) {
}

WritePasswordJob::~WritePasswordJob() {
}

void WritePasswordJob::setBinaryData( const QByteArray& data ) {
    WritePasswordJobPrivate *priv = dynamic_cast<WritePasswordJobPrivate*>(d);
    priv->data = data;
}

void WritePasswordJob::setTextData( const QString& data ) {
    WritePasswordJobPrivate *priv = dynamic_cast<WritePasswordJobPrivate*>(d);
    priv->data = data.toUtf8();
}

DeletePasswordJob::DeletePasswordJob( const QString& service, QObject* parent )
    : Job( service, new DeletePasswordJobPrivate( service, this ), parent ) {
}

DeletePasswordJob::~DeletePasswordJob() {
}

DeletePasswordJobPrivate::DeletePasswordJobPrivate(const QString& service_, DeletePasswordJob* qq) :
    JobPrivate(service_, qq)
{

}

JobExecutor::JobExecutor()
    : QThread( 0 )
{
    m_ready.lock();
}

void JobExecutor::enqueue( Job* job ) {
    QMutexLocker ml(&m_mux);
    m_queue.enqueue( job );
    m_condition.wakeAll();
}

bool JobExecutor::waitForReady() {
    m_ready.lock();
    return true;
}

void JobExecutor::run() {
    m_ready.unlock();
    do {
        QMutexLocker ml(&m_mux);
        m_condition.wait(&m_mux);

        QPointer<Job> next;
        while ( !next && !m_queue.isEmpty() ) {
            next = m_queue.dequeue();
        }
        if ( next ) {
            next->d->scheduledStart();
        }
    } while(!isInterruptionRequested());
}

JobExecutor* JobExecutor::s_instance = 0;

JobExecutor* JobExecutor::instance() {
    if ( !s_instance ) {
        s_instance = new JobExecutor;
        s_instance->start();
        s_instance->waitForReady();
    }
    return s_instance;
}

ReadPasswordJobPrivate::ReadPasswordJobPrivate(const QString &service_, ReadPasswordJob *qq) :
    JobPrivate(service_, qq),
    walletHandle( 0 )
{

}

WritePasswordJobPrivate::WritePasswordJobPrivate(const QString &service_, WritePasswordJob *qq) :
    JobPrivate(service_, qq)
{

}

JobPrivate::JobPrivate(const QString &service_, Job *qq)
    : error( NoError )
    , service( service_ )
    , autoDelete( true )
    , insecureFallback( false )
    , q ( qq )
{

}
