// rmpp-bt: Bluetooth settings page for reMarkable Paper Pro (xochitl), loaded with LD_PRELOAD.
// Injects a "Bluetooth" row under Wi-Fi in Settings and a page that talks to BlueZ over D-Bus.
// Built without moc: QML talks to C++ through a QQmlPropertyMap ("bt").
#include <functional>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRandomGenerator>
#include <QtCore/QResource>
#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlPropertyMap>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;
Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjects)

namespace {

const QString BLUEZ = QStringLiteral("org.bluez");
const QString ADAPTER = QStringLiteral("/org/bluez/hci0");
const QString AGENT_PATH = QStringLiteral("/rmppbt/agent");

QDBusConnection bus() { return QDBusConnection::systemBus(); }

class Bluetooth;
Bluetooth *g_bt = nullptr;

// org.bluez.Agent1, implemented without moc.
class Agent : public QDBusVirtualObject {
public:
    explicit Agent(Bluetooth *bt) : m_bt(bt) {}
    QString introspect(const QString &) const override {
        return QStringLiteral(
            "<interface name=\"org.bluez.Agent1\">"
            "<method name=\"Release\"/>"
            "<method name=\"RequestPinCode\"><arg type=\"o\" direction=\"in\"/><arg type=\"s\" direction=\"out\"/></method>"
            "<method name=\"DisplayPinCode\"><arg type=\"o\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/></method>"
            "<method name=\"RequestPasskey\"><arg type=\"o\" direction=\"in\"/><arg type=\"u\" direction=\"out\"/></method>"
            "<method name=\"DisplayPasskey\"><arg type=\"o\" direction=\"in\"/><arg type=\"u\" direction=\"in\"/><arg type=\"q\" direction=\"in\"/></method>"
            "<method name=\"RequestConfirmation\"><arg type=\"o\" direction=\"in\"/><arg type=\"u\" direction=\"in\"/></method>"
            "<method name=\"RequestAuthorization\"><arg type=\"o\" direction=\"in\"/></method>"
            "<method name=\"AuthorizeService\"><arg type=\"o\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/></method>"
            "<method name=\"Cancel\"/>"
            "</interface>");
    }
    bool handleMessage(const QDBusMessage &msg, const QDBusConnection &conn) override;

private:
    Bluetooth *m_bt;
};

class Bluetooth {
public:
    QQmlPropertyMap *map = nullptr;
    QTimer *poll = nullptr;
    QTimer *scanStop = nullptr;
    QObject *owner = nullptr;
    bool agentRegistered = false;

    explicit Bluetooth(QObject *parent) : owner(parent) {
        qDBusRegisterMetaType<InterfaceMap>();
        qDBusRegisterMetaType<ManagedObjects>();
        map = new QQmlPropertyMap(parent);
        for (const char *k : {"devices", "passkey", "pairingName", "status", "command"})
            map->insert(QString::fromLatin1(k), QString());
        map->insert(QStringLiteral("powered"), false);
        map->insert(QStringLiteral("available"), false);
        map->insert(QStringLiteral("discovering"), false);
        QObject::connect(map, &QQmlPropertyMap::valueChanged, parent,
                         [this](const QString &key, const QVariant &v) {
                             if (key == QLatin1String("command")) command(v.toString().section('#', 0, 0));
                         });
        poll = new QTimer(parent);
        QObject::connect(poll, &QTimer::timeout, parent, [this] { refresh(); });
        scanStop = new QTimer(parent);
        scanStop->setSingleShot(true);
        QObject::connect(scanStop, &QTimer::timeout, parent, [this] { adapterCall("StopDiscovery"); });
        bus().registerVirtualObject(AGENT_PATH, new Agent(this));
    }

    void set(const char *k, const QVariant &v) { map->insert(QString::fromLatin1(k), v); }

    void registerAgent() {
        if (agentRegistered) return;
        QDBusMessage m = QDBusMessage::createMethodCall(BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "RegisterAgent");
        m << QVariant::fromValue(QDBusObjectPath(AGENT_PATH)) << QStringLiteral("KeyboardDisplay");
        QDBusMessage r = bus().call(m);
        if (r.type() == QDBusMessage::ErrorMessage && r.errorName() != "org.bluez.Error.AlreadyExists") {
            qWarning("[rmpp-bt] RegisterAgent: %s", qPrintable(r.errorMessage()));
            return;
        }
        QDBusMessage d = QDBusMessage::createMethodCall(BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "RequestDefaultAgent");
        d << QVariant::fromValue(QDBusObjectPath(AGENT_PATH));
        bus().call(d);
        agentRegistered = true;
    }

    void adapterCall(const char *method) {
        bus().asyncCall(QDBusMessage::createMethodCall(BLUEZ, ADAPTER, "org.bluez.Adapter1", QString::fromLatin1(method)));
    }

    void setProperty(const QString &path, const QString &iface, const QString &prop, const QVariant &v) {
        QDBusMessage m = QDBusMessage::createMethodCall(BLUEZ, path, "org.freedesktop.DBus.Properties", "Set");
        m << iface << prop << QVariant::fromValue(QDBusVariant(v));
        bus().call(m);
    }

    // Call a Device1 method asynchronously; report errors in "status".
    void deviceCall(const QString &path, const QString &method, int timeoutMs, std::function<void()> onOk = {}) {
        QDBusMessage m = QDBusMessage::createMethodCall(BLUEZ, path, "org.bluez.Device1", method);
        auto *w = new QDBusPendingCallWatcher(bus().asyncCall(m, timeoutMs), owner);
        QObject::connect(w, &QDBusPendingCallWatcher::finished, owner, [this, w, method, onOk] {
            QDBusPendingReply<> r = *w;
            w->deleteLater();
            if (r.isError()) {
                set("passkey", QString());
                set("status", method + " failed: " + r.error().message());
            } else if (onOk) {
                onOk();
            }
            refresh();
        });
    }

    void command(const QString &c) {
        const QString verb = c.section(':', 0, 0), arg = c.section(':', 1);
        if (verb == "active") {
            if (arg == "1") { registerAgent(); refresh(); poll->start(1500); }
            else { poll->stop(); if (map->value("discovering").toBool()) adapterCall("StopDiscovery"); }
        } else if (verb == "power") {
            setProperty(ADAPTER, "org.bluez.Adapter1", "Powered", arg == "1");
            refresh();
        } else if (verb == "scan") {
            if (arg == "1") { set("status", QString()); adapterCall("StartDiscovery"); scanStop->start(30000); }
            else adapterCall("StopDiscovery");
        } else if (verb == "pair") {
            registerAgent();
            adapterCall("StopDiscovery");
            set("status", "Pairing…");
            set("pairingName", nameOf(arg));
            deviceCall(arg, "Pair", 120000, [this, arg] {
                set("passkey", QString());
                setProperty(arg, "org.bluez.Device1", "Trusted", true);
                set("status", "Paired. Connecting…");
                deviceCall(arg, "Connect", 30000, [this] { set("status", QString()); });
            });
        } else if (verb == "connect") {
            set("status", "Connecting… press a key on the keyboard");
            deviceCall(arg, "Connect", 30000, [this] { set("status", QString()); });
        } else if (verb == "disconnect") {
            deviceCall(arg, "Disconnect", 15000);
        } else if (verb == "remove") {
            QDBusMessage m = QDBusMessage::createMethodCall(BLUEZ, ADAPTER, "org.bluez.Adapter1", "RemoveDevice");
            m << QVariant::fromValue(QDBusObjectPath(arg));
            bus().call(m);
            refresh();
        }
    }

    QString nameOf(const QString &path) {
        const QJsonArray a = QJsonDocument::fromJson(map->value("devices").toString().toUtf8()).array();
        for (const auto &v : a)
            if (v.toObject().value("path").toString() == path) return v.toObject().value("name").toString();
        return {};
    }

    void refresh() {
        QDBusMessage m = QDBusMessage::createMethodCall(BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        QDBusMessage r = bus().call(m, QDBus::Block, 5000);
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) {
            set("available", false);
            set("powered", false);
            return;
        }
        const ManagedObjects objs = qdbus_cast<ManagedObjects>(r.arguments().first());
        bool haveAdapter = false;
        QJsonArray devs;
        for (auto it = objs.cbegin(); it != objs.cend(); ++it) {
            const QString path = it.key().path();
            if (it.value().contains("org.bluez.Adapter1") && path == ADAPTER) {
                const QVariantMap a = it.value().value("org.bluez.Adapter1");
                haveAdapter = true;
                set("powered", a.value("Powered").toBool());
                set("discovering", a.value("Discovering").toBool());
            }
            if (!it.value().contains("org.bluez.Device1") || !path.startsWith(ADAPTER + "/")) continue;
            const QVariantMap d = it.value().value("org.bluez.Device1");
            QJsonObject o;
            o["path"] = path;
            o["address"] = d.value("Address").toString();
            o["name"] = d.value("Name").toString();
            o["paired"] = d.value("Paired").toBool();
            o["connected"] = d.value("Connected").toBool();
            o["icon"] = d.value("Icon").toString();
            devs.append(o);
        }
        set("available", haveAdapter);
        if (!haveAdapter) set("powered", false);
        const QString json = QString::fromUtf8(QJsonDocument(devs).toJson(QJsonDocument::Compact));
        if (map->value("devices").toString() != json) set("devices", json);
    }
};

bool Agent::handleMessage(const QDBusMessage &msg, const QDBusConnection &conn) {
    const QString m = msg.member();
    const QList<QVariant> a = msg.arguments();
    auto show = [this](const QString &code) { m_bt->set("passkey", code); };
    if (m == "DisplayPasskey" && a.size() >= 2) {
        show(QStringLiteral("%1").arg(a.at(1).toUInt(), 6, 10, QChar('0')));
        conn.send(msg.createReply());
    } else if (m == "RequestConfirmation" && a.size() >= 2) {
        show(QStringLiteral("%1").arg(a.at(1).toUInt(), 6, 10, QChar('0')));
        conn.send(msg.createReply());
    } else if (m == "RequestPinCode") {
        const QString pin = QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(1000000), 6, 10, QChar('0'));
        show(pin);
        conn.send(msg.createReply(pin));
    } else if (m == "DisplayPinCode" && a.size() >= 2) {
        show(a.at(1).toString());
        conn.send(msg.createReply());
    } else if (m == "RequestPasskey") {
        const uint pk = QRandomGenerator::global()->bounded(1000000);
        show(QStringLiteral("%1").arg(pk, 6, 10, QChar('0')));
        conn.send(msg.createReply(QVariant::fromValue(pk)));
    } else if (m == "Cancel" || m == "Release") {
        show(QString());
        conn.send(msg.createReply());
    } else {  // RequestAuthorization, AuthorizeService: accept
        conn.send(msg.createReply());
    }
    return true;
}

// Depth-first search of the visual item tree (QML items are not always QObject children).
QQuickItem *findItem(QQuickItem *root, const QString &name, int depth = 0) {
    if (!root || depth > 200) return nullptr;
    if (root->objectName() == name) return root;
    const auto kids = root->childItems();
    for (QQuickItem *k : kids)
        if (QQuickItem *f = findItem(k, name, depth + 1)) return f;
    return nullptr;
}

QQuickItem *findByName(QObject *root, const char *name) {
    if (!root) return nullptr;
    const QString n = QString::fromLatin1(name);
    if (auto *item = qobject_cast<QQuickItem *>(root))
        if (QQuickItem *f = findItem(item, n)) return f;
    if (auto *it = root->findChild<QQuickItem *>(n)) return it;
    return nullptr;
}

QQuickItem *createFrom(QQmlEngine *eng, QQmlContext *ctx, const char *url, QQuickItem *parent) {
    QQmlComponent comp(eng, QUrl(QString::fromLatin1(url)));
    QObject *o = comp.create(ctx);
    if (!o) {
        for (const auto &e : comp.errors()) qWarning("[rmpp-bt] %s", qPrintable(e.toString()));
        return nullptr;
    }
    auto *item = qobject_cast<QQuickItem *>(o);
    if (!item) { delete o; return nullptr; }
    item->setParentItem(parent);
    item->setParent(parent);
    return item;
}

void tryInject() {
    const auto windows = QGuiApplication::allWindows();
    static int ticks = 0;
    if (qEnvironmentVariableIntValue("RMPPBT_DEBUG") && (++ticks % 10) == 1)
        for (QWindow *w : windows) {
            auto *qw = qobject_cast<QQuickWindow *>(w);
            qWarning("[rmpp-bt] window %s visible=%d quick=%d root=%s", w->metaObject()->className(), w->isVisible(),
                     qw != nullptr, qw && !qw->contentItem()->childItems().isEmpty()
                         ? qw->contentItem()->childItems().first()->metaObject()->className() : "-");
        }
    for (QWindow *w : windows) {
        auto *qw = qobject_cast<QQuickWindow *>(w);
        if (!qw) continue;
        QQuickItem *col = findByName(qw->contentItem(), "settingsColumn");
        if (!col) col = findByName(qw, "settingsColumn");
        if (!col || col->property("_rmppbt").toBool()) continue;
        col->setProperty("_rmppbt", true);

        QQuickItem *settings = col;
        while (settings && settings->objectName() != QLatin1String("Settings")) settings = settings->parentItem();
        QQuickItem *loader = settings ? findByName(settings, "payloadLoader") : nullptr;
        QQmlEngine *eng = settings ? qmlEngine(settings) : nullptr;
        QQmlContext *base = settings ? qmlContext(settings) : nullptr;
        if (!settings || !loader || !eng || !base) {
            qWarning("[rmpp-bt] settings page found but parts missing (settings=%d loader=%d engine=%d)",
                     settings != nullptr, loader != nullptr, eng != nullptr);
            continue;
        }
        auto *ctx = new QQmlContext(base, settings);
        ctx->setContextProperty("bt", g_bt->map);
        ctx->setContextProperty("settingsRoot", settings);
        ctx->setContextProperty("payloadLoaderRef", loader);

        QQuickItem *row = createFrom(eng, ctx, "qrc:/rmppbt/qml/BtSidebarItem.qml", col);
        if (row) {
            const auto kids = col->childItems();
            for (QQuickItem *k : kids)
                if (k != row && k->property("iconSource").toString().contains(QLatin1String("wifi"))) {
                    row->stackAfter(k);
                    break;
                }
        }
        QQuickItem *page = createFrom(eng, ctx, "qrc:/rmppbt/qml/BtPage.qml", settings);
        qWarning("[rmpp-bt] injected into Settings: row=%s page=%s", row ? "ok" : "FAILED", page ? "ok" : "FAILED");
    }
}

void rmppbtInit() {
    const QString rcc = qEnvironmentVariable("RMPPBT_RCC", QStringLiteral("/usr/lib/rmpp-bt/rmpp-bt.rcc"));
    const bool ok = QResource::registerResource(rcc);
    qWarning("[rmpp-bt] resources %s: %s", qPrintable(rcc), ok ? "registered" : "FAILED");
    auto *app = QCoreApplication::instance();
    g_bt = new Bluetooth(app);
    auto *t = new QTimer(app);
    QObject::connect(t, &QTimer::timeout, app, [] { tryInject(); });
    t->start(1000);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(rmppbtInit)
