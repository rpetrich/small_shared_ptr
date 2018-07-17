#include "small_shared_ptr.h"

#include <string>

struct point2d {
	int x;
	int y;
};

struct point3d: public point2d {
	int z;
};

struct cycle_attempt {
	small_shared_ptr<cycle_attempt> other;
	cycle_attempt(small_shared_ptr<cycle_attempt> other_) : other(other_) {
	}
};

int main() {
	auto foo = make_small_shared<std::string>("hello world!");
	auto bar = std::move(foo);
	auto baz = bar;
	foo = nullptr;
	bar = nullptr;
	small_shared_ptr<point3d> p3 = make_small_shared<point3d>();
	p3->x = 0;
	p3->y = 0;
	p3->z = 0;
	small_shared_ptr<point3d> p2 = std::move(p3);;
	p3 = nullptr;
	//p2 = make_small_shared<point3d>();
	auto attempt1 = make_small_shared<cycle_attempt>(nullptr);
	auto attempt2 = make_small_shared<cycle_attempt>(attempt1);
	small_weak_ptr<cycle_attempt> weak = attempt1;
	attempt1 = nullptr;
	attempt2 = nullptr;
	weak.expired();
	// attempt2->other = attempt1;
	// attempt2->other = std::move(attempt1);
	return 0;
}
