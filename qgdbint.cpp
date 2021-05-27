#include "qgdbint.h"
#include "record.h"
#include <QEventLoop>

namespace qgdbint {

QGdbProcessManager::~QGdbProcessManager() {
	clear();
}

void QGdbProcessManager::prepare(QString program, QStringList arguments,
							 QString gdbPath, QString gdbServerPath, int port) {
	gdbServer = new QProcess(this);
	gdbServer->setProgram(gdbServerPath);
	gdb = new QProcess(this);
	gdb->setProgram(gdbPath);

	// TODO: findout what cause the problem: GDB Version or System
#ifdef Q_OS_WIN32
	arguments.push_front(program);
#endif
	gdbServer->setArguments(QStringList() << "--no-startup-with-shell" << "--once" << QString(":%1").arg(port) << program << arguments);
	gdb->setArguments({ "-i=mi" });

	connect(gdb, &QProcess::readyReadStandardOutput, this, &QGdbProcessManager::onReadyRead);
	connect(gdbServer, &QProcess::readyReadStandardOutput, this, &QGdbProcessManager::onDispatchStdout);
	connect(gdbServer, &QProcess::readyReadStandardError, this, &QGdbProcessManager::onDispatchStderr);
	connect(gdb, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SIGNAL(errorOccurred(QProcess::ProcessError)));
	connect(gdb, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &QGdbProcessManager::onFinished);
}

void QGdbProcessManager::clear() {
	terminate();
	delete gdb;
	delete gdbServer;
}

void QGdbProcessManager::run() {
	buffer.clear();
	gdbServer->start();
	gdb->start();
}

void QGdbProcessManager::exec(QString cmd) {
	gdb->write(QString("%1\n").arg(cmd).toUtf8());
}

void QGdbProcessManager::terminate() {
	gdb->kill();
	gdbServer->kill();
}

void QGdbProcessManager::input(QString buffer) {
	gdbServer->write(buffer.toUtf8());
}

void QGdbProcessManager::inputFin() {
	gdbServer->closeWriteChannel();
}

void QGdbProcessManager::onReadyRead() {
	buffer += gdb->readAllStandardOutput();
	int pos = buffer.indexOf("(gdb) \n");
	if (pos != -1) {
		QString data = QString::fromUtf8(buffer.left(pos));
		buffer = buffer.mid(pos + 6);
		emit Record(data.split('\n', Qt::SkipEmptyParts));
	}
}

void QGdbProcessManager::onDispatchStdout() {
	emit readyStdout(QString::fromUtf8(gdbServer->readAllStandardOutput()));
}

void QGdbProcessManager::onDispatchStderr() {
	emit readyStderr(QString::fromUtf8(gdbServer->readAllStandardError()));
}

void QGdbProcessManager::onFinished() {
	buffer += gdb->readAllStandardOutput();
	emit Record(QString::fromUtf8(buffer).split('\n', Qt::SkipEmptyParts));
}

QGdb::QGdb(QString gdbPath, QString gdbServerPath, int port, QObject* parent)
	: QObject(parent), gdb(gdbPath), gdbServer(gdbServerPath), port(port) {
	manager = new QGdbProcessManager(this);
	QObject::connect(manager, &QGdbProcessManager::Record, this, &QGdb::onRecord);
	QObject::connect(manager, SIGNAL(readyStdout(QString)), this, SIGNAL(readyStdout(QString)));
	QObject::connect(manager, SIGNAL(readyStderr(QString)), this, SIGNAL(readyStderr(QString)));
	defHandle = [this](QStringList record) {
		QString resp;
		Const cst;
		for (auto str : filter(record, '~')) {
			cst.parse(str);
			resp += cst.value;
		}
		emit textResponse(resp, this);
		QStringList stateRecord = filter(record, '*');
		if (stateRecord.size()) {
			Record rec(stateRecord.back());
			bool newState = rec.resultClass == "running";
			if (newState != state) {
				emit stateChanged(state = newState, rec.result.locate("reason")->str(), this); // when state is running, there is no `reason', thus set to ""
				if (!newState) {
					auto frame = rec.result.locate("frame");
					QString file = frame->locate("fullname")->str();
					if (file.size()) {
						emit positionUpdated(file, frame->locate("line")->str().toInt(), this);
					}
				}
			}
		}
		QStringList resultRecord = filter(record, '^');
		for (auto record : resultRecord) {
			Record rec(record);
			if (rec.resultClass == "error") {
				emit errorOccurered(rec.result.locate("msg")->str());
			}
		}
	};
}

QString QGdb::waitUntilPause() {
	QEventLoop loop(this);
	QString reason;
	auto connection = QObject::connect(this, &QGdb::stateChanged, [&loop, &reason](bool running, QString r) {
		if (running == false) {
			reason = r;
			loop.quit();
		}
	});
	loop.exec();
	disconnect(connection);
	return reason;
}

void QGdb::input(QString buffer, bool finish) {
	manager->input(buffer);
	if (finish) {
		finishInput();
	}
}

void QGdb::finishInput() {
	manager->inputFin();
}

void QGdb::start(QString program, QStringList arguments) {
	manager->prepare(program, arguments, gdb, gdbServer, port);
	manager->run();
	QEventLoop loop(this);
	QObject::connect(manager, &QGdbProcessManager::Record, &loop, &QEventLoop::quit);
	loop.exec(); // skip the first part.
	state = false;
	manager->exec(QString("-target-select remote :%1").arg(port));
	manager->exec(QString("-file-exec-and-symbols \"%1\"").arg(program.replace('\\', "\\\\").replace('"', "\\\"")));
}

void QGdb::cont() {
	manager->exec("-exec-continue");
}

void QGdb::exit() {
	manager->exec("-gdb-exit");
	QEventLoop loop(this);
	reqHandle = [this, &loop](QStringList) {
		emit exited(this);
		loop.exit();
	};
	loop.exec();
}

int QGdb::setBreakpoint(int row) {
	manager->exec(QString("-break-insert -f %1").arg(row));
	QEventLoop loop(this);
	reqHandle = [&loop](QStringList record) {
		Record rec(filter(record, '^').first());
		if (rec.resultClass == "error") {
			loop.exit(-1);
		} else {
			loop.exit(rec.result.locate("bkpt")->locate("number")->str().toInt());
		}
	};
	return loop.exec();
}

int QGdb::setBreakpoint(QString func, int* row) {
	manager->exec(QString("-break-insert -f %1").arg(func));
	QEventLoop loop(this);
	reqHandle = [&loop, row](QStringList record) {
		Record rec(filter(record, '^').first());
		if (rec.resultClass == "error") {
			loop.exit(-1);
		} else {
			auto bkpt = rec.result.locate("bkpt")->as<Tuple>();
			if (row) {
				*row = bkpt->locate("line")->str().toInt();
			}
			loop.exit(bkpt->locate("number")->str().toInt());
		}
	};
	return loop.exec();
}

void QGdb::delBreakpoint(int id) {
	manager->exec(QString("-break-delete %1").arg(id));
}

void QGdb::delAllBreakpoints() {
	manager->exec(QString("-break-delete"));
}

void QGdb::disableBreakpoint(int id) {
	manager->exec(QString("-break-disable %1").arg(id));
}

void QGdb::enableBreakpoint(int id) {
	manager->exec(QString("-break-enable %1").arg(id));
}

void QGdb::step() {
	manager->exec("-exec-next");
}

void QGdb::stepIn() {
	manager->exec("-exec-step");
}

void QGdb::stepOut() {
	manager->exec("-exec-finish");
}

QString QGdb::eval(QString expr) {
	QString result;
	manager->exec(QString("-data-evaluate-expression %1").arg(expr));
	QEventLoop loop(this);
	reqHandle = [&loop, &result](QStringList record) {
		Record rec(filter(record, '^').first());
		rec.result.dump();
		if (rec.resultClass == "error") {
			loop.exit(-1);
		} else {
			auto val = rec.result.locate("value")->as<Const>();
			result = val->str();
			loop.exit(0);
		}
	};
	loop.exec();
	return result;
}

void QGdb::terminate() {
	manager->terminate();
}

void QGdb::onRecord(QStringList record) {
	defHandle(record);
	if (reqHandle) {
		reqHandle(record);
		decltype(reqHandle) empty;
		reqHandle.swap(empty);
	}
}

QStringList QGdb::filter(QStringList record, QChar head) {
	QStringList result;
	for (auto str : record) {
		if (str[0] == head) {
			result.push_back(str.mid(1)); // take first char
		}
	}
	return result;
}

}
