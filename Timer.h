#pragma once
#include <chrono>
#include <stack>
#include <string>
#include <iostream>

struct Timer {
	std::chrono::steady_clock::time_point t0;
	std::chrono::steady_clock::time_point t1;
};

std::stack<Timer> time_stack;

void StartTimer() {
	Timer t;
	t.t0 = std::chrono::high_resolution_clock::now();
	time_stack.push(t);
}
float EndTimer(std::string message) {
	if (time_stack.empty()) {
		return -1;
	}
	Timer t = time_stack.top();
	t.t1 = std::chrono::high_resolution_clock::now();
	auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t.t1 - t.t0).count() / 1000.0f;
	if (message.length() > 0) {
		std::cout << message << " " << dur << " ms" << std::endl;
	}
	time_stack.pop();
	return dur;
}
