#include "record.h"
#include <QDebug>

namespace qgdbint {

Const Const::defaultValue;
Tuple Tuple::defaultValue;

QString Value::str() const {
	return as<Const>()->value;
}

QSharedPointer<Value> Value::locate(QString key) const {
	return as<Tuple>()->locate(key);
}

int Const::parse(QString str) {
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
			value = QString::fromUtf8(s);
			return ptr + 1;
		default:
			s.push_back(str[ptr++].unicode());
		}
	}
	return -1;
}

void Const::dump() const {
	qDebug() << value;
}

static QSharedPointer<Value> detectValue(QString str, int& ret) {
	Value* value = nullptr;
	if (str[0] == '"') {
		value = new Const;
	} else if (str[0] == '{' || str[0] == '[') {
		value = new Tuple;
	}
	ret = value->parse(str);
	return QSharedPointer<Value>(value);
}

int Tuple::parse(QString str) {
	bool isList = str[0].unicode() == '[';
	char endC = (isList ? ']' : '}');
	bool isValueList = isList && QString("{[\"").indexOf(str[1]) != -1;
	int ptr = 1, offs;
	Result rs;
	while (str[ptr] != endC) {
		if (!isValueList) {
			int p = str.indexOf('=', ptr);
			rs.key = str.mid(ptr, p - ptr);
			ptr = p + 1;
		}
		rs.value = detectValue(str.mid(ptr), offs);
		value.push_back(rs);
		ptr += offs;
		if (str[ptr] == ',') {
			++ptr;
		}
	}
	return ptr + 1;
}

QSharedPointer<Value> Tuple::locate(QString key) const {
	for (const auto& r : value) {
		if (r.key == key) {
			return r.value;
		}
	}
	return QSharedPointer<Value>();
}

void Tuple::dump() const {
	qDebug() << '{';
	for (const auto& r : value) {
		r.dump();
	}
	qDebug() << '}';
}

void Result::dump() const {
	qDebug() << key << '=';
	value->dump();
}

Record::Record(QString str) {
	int pos = str.indexOf(',');
	if (pos == -1) {
		resultClass = str;
	} else {
		resultClass = str.left(pos);
		result.parse(QString("{%1}").arg(str.mid(pos + 1)));
	}
}

}
