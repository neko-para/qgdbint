#include "qgdbint.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

using namespace qgdbint;

int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);
	QTimer::singleShot(0, [&]() {
		QGdb* gdb = new QGdb("/usr/bin/gdb", "/usr/bin/gdbserver");
		QObject::connect(gdb, &qgdbint::QGdb::textResponse, [](QString text) {
			qDebug().noquote() << text;
		});
		QObject::connect(gdb, &qgdbint::QGdb::stateChanged, [](bool running, QString reason, QGdb* gdb) {
			if (running == false && reason == "exited-normally") {
				gdb->exit();
			}
		});
		QObject::connect(gdb, &qgdbint::QGdb::exited, &app, &QCoreApplication::quit);
		gdb->start("/home/nekosu/test");
		if (gdb->connect()) {
			gdb->cont();
		}
	});
	return app.exec();
}
