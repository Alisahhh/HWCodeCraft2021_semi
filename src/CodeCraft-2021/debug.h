//
// Created by kskun on 2021/3/30.
//
#pragma once

#define LOG_TIME(time) std::clog << (double) (time) / CLOCKS_PER_SEC * 1000 << "ms" << std::endl;
#define LOG_TIME_SEC(time) std::clog << (double) (time) / CLOCKS_PER_SEC << "s" << std::endl;
