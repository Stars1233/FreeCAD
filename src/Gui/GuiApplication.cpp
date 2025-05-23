/***************************************************************************
 *   Copyright (c) 2015 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <sstream>
# include <QAbstractSpinBox>
# include <QByteArray>
# include <QComboBox>
# include <QTextStream>
# include <QFileInfo>
# include <QFileOpenEvent>
# include <QSessionManager>
# include <QTimer>
#endif

#include <QLocalServer>
#include <QLocalSocket>

#if defined(Q_OS_UNIX)
# include <sys/types.h>
# include <ctime>
# include <unistd.h>
#endif

#include <App/Application.h>
#include <Base/Console.h>
#include <Base/Exception.h>

#include "GuiApplication.h"
#include "Application.h"
#include "MainWindow.h"
#include "SpaceballEvent.h"


using namespace Gui;

GUIApplication::GUIApplication(int & argc, char ** argv)
    : GUIApplicationNativeEventAware(argc, argv)
{
    connect(this, &GUIApplication::commitDataRequest,
            this, &GUIApplication::commitData, Qt::DirectConnection);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    setFallbackSessionManagementEnabled(false);
#endif
}

GUIApplication::~GUIApplication() = default;

bool GUIApplication::notify (QObject * receiver, QEvent * event)
{
    if (!receiver) {
        Base::Console().log("GUIApplication::notify: Unexpected null receiver, event type: %d\n",
            (int)event->type());
        return false;
    }

    // https://github.com/FreeCAD/FreeCAD/issues/16905
    std::string exceptionWarning =
#if FC_DEBUG
        "Exceptions must be caught before they go through Qt."
        " Ignoring this will cause crashes on some systems.\n";
#else
        "";
#endif

    try {
        if (event->type() == Spaceball::ButtonEvent::ButtonEventType ||
            event->type() == Spaceball::MotionEvent::MotionEventType)
            return processSpaceballEvent(receiver, event);
        else
            return QApplication::notify(receiver, event);
    }
    catch (const Base::SystemExitException &e) {
        caughtException.reset(new Base::SystemExitException(e));
        qApp->exit(e.getExitCode());
        return true;
    }
    catch (const Base::Exception& e) {
        Base::Console().error("Unhandled Base::Exception caught in GUIApplication::notify.\n"
                              "The error message is: %s\n%s", e.what(), exceptionWarning);
    }
    catch (const std::exception& e) {
        Base::Console().error("Unhandled std::exception caught in GUIApplication::notify.\n"
                              "The error message is: %s\n%s", e.what(), exceptionWarning);
    }
    catch (...) {
        Base::Console().error("Unhandled unknown exception caught in GUIApplication::notify.\n%s",
                              exceptionWarning);
    }

    // Print some more information to the log file (if active) to ease bug fixing
    try {
        std::stringstream dump;
        dump << "The event type " << (int)event->type() << " was sent to "
             << receiver->metaObject()->className() << "\n";
        dump << "Object tree:\n";
        if (receiver->isWidgetType()) {
            QWidget* w = qobject_cast<QWidget*>(receiver);
            while (w) {
                dump << "\t";
                dump << w->metaObject()->className();
                QString name = w->objectName();
                if (!name.isEmpty())
                    dump << " (" << (const char*)name.toUtf8() << ")";
                w = w->parentWidget();
                if (w)
                    dump << " is child of\n";
            }
            std::string str = dump.str();
            Base::Console().log("%s",str.c_str());
        }
    }
    catch (...) {
        Base::Console().log("Invalid recipient and/or event in GUIApplication::notify\n");
    }

    return true;
}

void GUIApplication::commitData(QSessionManager &manager)
{
    if (manager.allowsInteraction()) {
        if (!Gui::getMainWindow()->close()) {
            // cancel the shutdown
            manager.release();
            manager.cancel();
        }
    }
    else {
        // no user interaction allowed, thus close all documents and
        // the main window
        App::GetApplication().closeAllDocuments();
        Gui::getMainWindow()->close();
    }
}

bool GUIApplication::event(QEvent * ev)
{
    if (ev->type() == QEvent::FileOpen) {
        // (macOS workaround when opening FreeCAD by opening a .FCStd file in 1.0)
        // With the current implementation of the splash screen boot procedure, Qt will
        // start an event loop before FreeCAD is fully initialized. This event loop will
        // process the QFileOpenEvent that is sent by macOS before the main window is ready.
        if (!Gui::getMainWindow()->property("eventLoop").toBool()) {
            // If we never reach this point when opening FreeCAD by double clicking an
            // .FCStd file, then the workaround isn't needed anymore and can be removed
            QEvent* eventCopy = new QFileOpenEvent(static_cast<QFileOpenEvent*>(ev)->file());
            QTimer::singleShot(0, [eventCopy, this]() {
                QCoreApplication::postEvent(this, eventCopy);
            });
            return true;
        }

        QString file = static_cast<QFileOpenEvent*>(ev)->file();
        QFileInfo fi(file);
        if (fi.suffix().toLower() == QLatin1String("fcstd")) {
            QByteArray fn = file.toUtf8();
            Application::Instance->open(fn, "FreeCAD");
            return true;
        }
    }

    return GUIApplicationNativeEventAware::event(ev);
}

// ----------------------------------------------------------------------------

class GUISingleApplication::Private {
public:
    explicit Private(GUISingleApplication *q_ptr)
      : q_ptr(q_ptr)
      , timer(new QTimer(q_ptr))
    {
        timer->setSingleShot(true);
        std::string exeName = App::Application::getExecutableName();
        serverName = QString::fromStdString(exeName);
    }

    ~Private()
    {
        if (server)
            server->close();
        delete server;
    }

    void setupConnection()
    {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(1000)) {
            this->running = true;
        }
        else {
            startServer();
        }
    }

    void startServer()
    {
        // Start a QLocalServer to listen for connections
        server = new QLocalServer();
        QObject::connect(server, &QLocalServer::newConnection,
                         q_ptr, &GUISingleApplication::receiveConnection);
        // first attempt
        if (!server->listen(serverName)) {
            if (server->serverError() == QAbstractSocket::AddressInUseError) {
                // second attempt
                server->removeServer(serverName);
                server->listen(serverName);
            }
        }
        if (server->isListening()) {
            Base::Console().log("Local server '%s' started\n", qPrintable(serverName));
        }
        else {
            Base::Console().log("Local server '%s' failed to start\n", qPrintable(serverName));
        }
    }

    GUISingleApplication *q_ptr;
    QTimer *timer;
    QLocalServer *server{nullptr};
    QString serverName;
    QList<QString> messages;
    bool running{false};
};

GUISingleApplication::GUISingleApplication(int & argc, char ** argv)
    : GUIApplication(argc, argv),
      d_ptr(new Private(this))
{
    d_ptr->setupConnection();
    connect(d_ptr->timer, &QTimer::timeout, this, &GUISingleApplication::processMessages);
}

GUISingleApplication::~GUISingleApplication() = default;

bool GUISingleApplication::isRunning() const
{
    return d_ptr->running;
}

bool GUISingleApplication::sendMessage(const QString &message, int timeout)
{
    QLocalSocket socket;
    bool connected = false;
    for(int i = 0; i < 2; i++) {
        socket.connectToServer(d_ptr->serverName);
        connected = socket.waitForConnected(timeout/2);
        if (connected || i > 0) {
            break;
        }
        int ms = 250;
#if defined(Q_OS_WIN)
        Sleep(DWORD(ms));
#else
        usleep(ms*1000);
#endif
    }
    if (!connected) {
        return false;
    }

    QTextStream ts(&socket);
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    ts.setCodec("UTF-8");
#else
    ts.setEncoding(QStringConverter::Utf8);
#endif
#if QT_VERSION <= QT_VERSION_CHECK(5, 15, 0)
    ts << message << endl;
#else
    ts << message << Qt::endl;
#endif

    return socket.waitForBytesWritten(timeout);
}

void GUISingleApplication::readFromSocket()
{
    auto socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        QTextStream in(socket);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        in.setCodec("UTF-8");
#else
        in.setEncoding(QStringConverter::Utf8);
#endif
        while (socket->canReadLine()) {
            d_ptr->timer->stop();
            QString message = in.readLine();
            Base::Console().log("Received message: %s\n", message.toStdString());
            d_ptr->messages.push_back(message);
            d_ptr->timer->start(1000);
        }
    }
}

void GUISingleApplication::receiveConnection()
{
    QLocalSocket *socket = d_ptr->server->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QLocalSocket::disconnected,
            socket, &QLocalSocket::deleteLater);
    connect(socket, &QLocalSocket::readyRead, this, &GUISingleApplication::readFromSocket);
}

void GUISingleApplication::processMessages()
{
    QList<QString> msg = d_ptr->messages;
    d_ptr->messages.clear();
    Q_EMIT messageReceived(msg);
}

// ----------------------------------------------------------------------------

WheelEventFilter::WheelEventFilter(QObject* parent)
  : QObject(parent)
{
}

bool WheelEventFilter::eventFilter(QObject* obj, QEvent* ev)
{
    if (qobject_cast<QComboBox*>(obj) && ev->type() == QEvent::Wheel)
        return true;
    auto sb = qobject_cast<QAbstractSpinBox*>(obj);
    if (sb) {
        if (ev->type() == QEvent::Show) {
            sb->setFocusPolicy(Qt::StrongFocus);
        }
        else if (ev->type() == QEvent::Wheel) {
            return !sb->hasFocus();
        }
    }
    return false;
}

#include "moc_GuiApplication.cpp"
