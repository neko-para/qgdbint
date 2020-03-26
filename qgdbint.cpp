#include "qgdbint.h"
#include <QRegularExpression>
#include <QDebug>
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

	gdbServer->setArguments(QStringList() << QString(":%1").arg(port) << program << arguments);
	gdb->setArguments({ "-i=mi" });

	connect(gdb, &QProcess::readyReadStandardOutput, this, &QGdbProcessManager::onReadyRead);
	connect(gdbServer, &QProcess::readyReadStandardOutput, this, &QGdbProcessManager::onDispatchStdout);
	connect(gdbServer, &QProcess::readyReadStandardError, this, &QGdbProcessManager::onDispatchStderr);
	connect(gdb, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SIGNAL(ErrorOccurred(QProcess::ProcessError)));
	connect(gdb, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &QGdbProcessManager::onFinished);
}

void QGdbProcessManager::clear() {
	terminate();
	delete gdb;
	delete gdbServer;
}

void QGdbProcessManager::run(QString input) {
	buffer.clear();
	gdbServer->start();
	gdbServer->write(input.toUtf8());
	gdbServer->closeWriteChannel();
	gdb->start();
}

void QGdbProcessManager::exec(QString cmd) {
	gdb->write(QString("%1\n").arg(cmd).toUtf8());
}

void QGdbProcessManager::terminate() {
	gdb->kill();
	gdbServer->kill();
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
	emit ReadyStdout(QString::fromUtf8(gdbServer->readAllStandardOutput()));
}

void QGdbProcessManager::onDispatchStderr() {
	emit ReadyStdout(QString::fromUtf8(gdbServer->readAllStandardError()));
}

void QGdbProcessManager::onFinished() {
	buffer += gdb->readAllStandardOutput();
	emit Record(QString::fromUtf8(buffer).split('\n', Qt::SkipEmptyParts));
}

QGdb::QGdb(QString gdbPath, QString gdbServerPath, int port, QObject* parent)
	: QObject(parent), gdb(gdbPath), gdbServer(gdbServerPath), port(port), state(true) {
	manager = new QGdbProcessManager(this);
	QObject::connect(manager, &QGdbProcessManager::Record, this, &QGdb::onRecord);
	reqHandle = defHandle = [this](QStringList record) {
		QString resp;
		for (auto str : filter(record, '~')) {
			resp += unescape(str);
		}
		emit textResponse(resp, this);
	};
}

void QGdb::waitUntilPause() {
	QEventLoop loop(this);
	QObject::connect(this, &QGdb::stateChanged, [&loop](bool running) {
		if (running == false) {
			loop.quit();
		}
	});
	loop.exec();
}

void QGdb::start(QString program, QStringList arguments, QString input) {
	manager->prepare(program, arguments, gdb, gdbServer, port);
	manager->run(input);
	QEventLoop loop(this);
	QObject::connect(manager, &QGdbProcessManager::Record, &loop, &QEventLoop::quit);
	loop.exec(); // skip the first part.
}

bool QGdb::connect() {
	manager->exec(QString("-target-select remote :%1").arg(port));
	QEventLoop loop(this);
	reqHandle = [this, &loop](QStringList record) {
		QString row = filter(record, '^').first();
		QString result = pickResult(row);
		if (result == "error") {
			qDebug() << parseKeyStringPair(row)["msg"];
		}
		defHandle(record);
		loop.exit(result == "connected");
	};
	return loop.exec();
}

void QGdb::cont() {
	manager->exec("-exec-continue");
}

void QGdb::exit() {
	manager->exec("-gdb-exit");
	QEventLoop loop(this);
	reqHandle = [this, &loop](QStringList record) {
		QString row = filter(record, '^').first();
		QString result = pickResult(row);
		if (result != "exit") {
			qDebug() << result;
		} else {
			emit exited(this);
		}
		loop.exit(0);
	};
	loop.exec();
}

void QGdb::onRecord(QStringList record) {
	QStringList rec = filter(record, '*');
	if (rec.size()) {
		QString row = rec.back();
		QString curState = pickResult(row);
		bool newState = curState == "running";
		if (newState != state) {
			emit stateChanged(state = newState, parseKeyStringPair(row)["reason"], this); // when state is running, there is no `reason', thus set to ""
		}
	}
	reqHandle(record);
	reqHandle = defHandle;
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
QString QGdb::unescape(QString str, QString* rest) {
	QByteArray s;
	int ptr = 1;
	for (; ptr < str.size(); ) {
		switch (str[ptr].unicode()) {
		case '\\':
			++ptr;
			switch (str[ptr].unicode()) {
#define _CASE_(c, tc) \
			case c: \
				s.push_back(tc); \
				++ptr; \
				break;
			_CASE_('\\', '\\')
			_CASE_('a', '\a')
			_CASE_('b', '\b')
			_CASE_('f', '\f')
			_CASE_('n', '\n')
			_CASE_('r', '\r')
			_CASE_('t', '\t')
			_CASE_('\'', '\'')
			_CASE_('"', '"')
#undef _CASE_
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7': { // \0 & \ddd
				int l = 0;
				int real = 0;
				while (l < 3 && unsigned(str[ptr + l].unicode() - '0') < 8) {
					real <<= 3;
					real |= str[ptr + l].unicode() - '0';
					++l;
				}
				s.push_back(real);
				ptr += l;
				break;
			}
			case 'x':{ // \xdd
				int l = 1;
				int real = 0;
				while (l < 3 && isxdigit(str[ptr + l].unicode())) {
					real <<= 4;
					int cd = str[ptr + l].unicode();
					real |= (isdigit(cd) ? cd - '0' : tolower(cd) - 'a' + 10);
					++l;
				}
				s.push_back(real);
				ptr += l;
				break;
			}
			default:
				;
			}
			break;
		case '"':
			if (rest) {
				*rest = str.mid(ptr + 1);
			}
			return QString::fromUtf8(s);
		default:
			s.push_back(str[ptr++].unicode());
		}
	}
	qDebug() << "error unescaping:" << str;
	return "";
}

QString QGdb::pickResult(QString &row) {
	int pos = row.indexOf(',');
	QString result = row.left(pos == -1 ? row.length() : pos);
	if (pos != -1) {
		row = row.mid(pos + 1);
	} else {
		row = "";
	}
	return result;
}

QMap<QString, QString> QGdb::parseKeyStringPair(QString row) {
	QMap<QString, QString> value;
	while (row.size()) {
		int pos = row.indexOf("=");
		if (pos == -1) {
			break;
		}
		QString rest;
		value[row.left(pos)] = unescape(row.mid(pos + 1), &rest);
		row = rest;
	}
	return value;
}

}
