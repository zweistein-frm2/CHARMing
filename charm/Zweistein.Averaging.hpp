#pragma once
#include <boost/circular_buffer.hpp>
namespace Zweistein {
	template<class T>
	class Averaging {
		boost::circular_buffer<T> cb;
	public:
		Averaging(int n) :cb(n) {}
		void addValue(T t) {
			cb.push_back(t);
		}
		T getAverage() {
			T r = 0;
			size_t s = cb.size();
			if (s == 0) return (T)0;
			for(T & t : cb) r += t;
			return r / s;
		}
		// fill the average with a value
	// the param number determines how often value is added (weight)
	// number should preferably be between 1 and size
		void fillValue(T value, int number) {
			size_t c = cb.capacity();
			if (number > c) number = c;
			for (int i = 0; i < number; i++) addValue(value);
		}

	};
}