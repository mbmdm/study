#pragma once
#include <iostream>
#include <ostream>
#include <exception>

using std::cout;
using std::endl;
using std::ostream;


/*
	определение макроса disable_copy отключает семантику копирования: копирующий   конструктор и оператор присваивания
	определение макроса disable_move отключает семантику перемещения: перемещающий конструктор и оператор присваивания
*/

class mclass {

	static int mclass_counter;
	int *resourse = nullptr;
	int id;

public:
	explicit mclass() {
		id = ++mclass_counter;
		resourse = new int{0};
		std::printf("mclass() -> #%d\n", id);
	}

	explicit mclass(int a) {
		id = ++mclass_counter;
		resourse = new int{ a };
		std::printf("mclass(int) -> #%d\n", id);
	}

#ifndef disable_move 

	mclass(mclass &&other) noexcept {
		id = ++mclass_counter;
		std::printf("mclass(&&) -> #%d from #%d\n", id, other.id);
		this->resourse = other.resourse;
		other.resourse = new int{0};
	}

	mclass &operator=(mclass &&other) {
		std::printf("operator=(&&) -> #%d from #%d\n", id, other.id);
		delete resourse;
		this->resourse = other.resourse;
		other.resourse = new int{ 0 };
		return *this;
	}

#else

	mclass(mclass &&other) = delete;
	mclass &operator=(mclass &&other) = delete;

#endif // !disable_move 

#ifndef disable_copy

	mclass(const mclass &other)  {
		id = ++mclass_counter;
		std::printf("mclass(const &) -> #%d from #%d\n", id, other.id);
		this->resourse = new int{ *other.resourse };
	}

	mclass &operator=(const mclass &other) {
		std::printf("operator=(const &) -> #%d from #%d\n", id, other.id);
		*this->resourse = *other.resourse;
		return *this;
	}

#else

	mclass(const mclass &other) = delete;
	mclass &operator=(const mclass &other) = delete;

#endif // !disable_copy

	~mclass() { 

		std::printf("~mclass() -> #%d\n", id);
		if (resourse) 
			delete resourse;
			//throw std::runtime_error{ "unxpected error: resource is null" };
	}

	void set_resource(const int a)  {

		if (resourse == nullptr)
			throw std::runtime_error{ "unxpected error: resource is null" };
		*resourse = a;
	}

	int get_resource() const { 

		if (resourse == nullptr)
			throw std::runtime_error{ "unxpected error: resource is null" };
		return *resourse; 
	}

	int instance_ID() const { return id; }

	bool operator<(const mclass &m) const {
		return this->id < m.id;
	}

	bool operator==(const mclass& m) const {
	
		if (*this->resourse == *m.resourse)
			return true;
		else return false;		
	}

	void print() const {

		if (resourse == nullptr)
			throw std::runtime_error{ "unxpected error: resource is null" };
		std::printf("mclass #%d {val = %d, addr: %d}\n", id, *resourse, resourse);
	}

	friend std::ostream& operator<<(std::ostream& out, const mclass& m) {

		out << "mclass #" << m.id << ",  addr: " << m.resourse;
		return out;
	}
};

int mclass::mclass_counter = 0;