#pragma once

#include <QSharedPointer>

namespace qgdbint {

struct Result;

struct Value {
	virtual int parse(QString str) = 0;
	virtual void dump() const = 0;
	template <typename T>
	T* as() {
		T* t = dynamic_cast<T*>(this);
		return t ? t : &T::defaultValue;
	}
	template <typename T>
	const T* as() const {
		const T* t = dynamic_cast<const T*>(this);
		return t ? t : &T::defaultValue;
	}
	QString str() const;
	QSharedPointer<Value> locate(QString key) const;
};

struct Const : public Value {
	static Const defaultValue;
	QString value;

	virtual int parse(QString str);
	virtual void dump() const;
};

struct Tuple : public Value {
	static Tuple defaultValue;
	QList<Result> value;

	virtual int parse(QString str);
	virtual void dump() const;
	QSharedPointer<Value> locate(QString key) const;
};

struct Result {
	QString key; // it seems that the VARIABLE(aka STRING) only contains a-z&-. i'm not sure whether digits are allowed.
	QSharedPointer<Value> value;

	void dump() const;
};

struct Record {
	QString resultClass;
	Tuple result;

	Record(QString str);
};

}
