/* Calf DSP Library
 * DSP primitives.
 *
 * Copyright (C) 2001-2007 Krzysztof Foltman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CALF_PRIMITIVES_H
#define __CALF_PRIMITIVES_H

#include <stack>
#include <map>
#include <cmath>

namespace dsp {

using namespace std;

inline void zero(float &v) {
    v = 0;
};

inline void zero(double &v) {
    v = 0;
};

inline void zero(uint32_t &v) { v = 0; };
inline void zero(uint16_t &v) { v = 0; };
inline void zero(uint8_t &v) { v = 0; };
inline void zero(int32_t &v) { v = 0; };
inline void zero(int16_t &v) { v = 0; };
inline void zero(int8_t &v) { v = 0; };

template<class T>
void zero(T *data, unsigned int size) {
    T value;
    dsp::zero(value);
    for (unsigned int i=0; i<size; i++)
        *data++ = value;
}

const double PI = 4.0 * atan(1.0);
    
template<class T = float>struct stereo_sample {
    T left;
    T right;
    /// default constructor - preserves T's semantics (ie. no implicit initialization to 0)
    inline stereo_sample() {
    }
    inline stereo_sample(T _left, T _right) {
        left = _left;
        right = _right;
    }
    inline stereo_sample(T _both) {
        left = right = _both;
    }
    template<typename U>
    inline stereo_sample(const stereo_sample<U> &value) {
        left = value.left;
        right = value.right;
    }
    inline stereo_sample& operator=(const T &value) {
        left = right = value;
        return *this;
    }
    template<typename U>
    inline stereo_sample& operator=(const stereo_sample<U> &value) {
        left = value.left;
        right = value.right;
        return *this;
    }
/*
    inline operator T() const {
        return (left+right)/2;
    }
*/
    inline stereo_sample& operator*=(const T &multiplier) {
        left *= multiplier;
        right *= multiplier;
        return *this;
    }
    inline stereo_sample& operator+=(const stereo_sample<T> &value) {
        left += value.left;
        right += value.right;
        return *this;
    }
    inline stereo_sample& operator-=(const stereo_sample<T> &value) {
        left -= value.left;
        right -= value.right;
        return *this;
    }
    template<typename U> inline stereo_sample<U> operator*(const U &value) const {
        return stereo_sample<U>(left*value, right*value);
    }
    /*inline stereo_sample<float> operator*(float value) const {
        return stereo_sample<float>(left*value, right*value);
    }
    inline stereo_sample<double> operator*(double value) const {
        return stereo_sample<double>(left*value, right*value);
    }*/
    inline stereo_sample<T> operator+(const stereo_sample<T> &value) {
        return stereo_sample(left+value.left, right+value.right);
    }
    inline stereo_sample<T> operator-(const stereo_sample<T> &value) {
        return stereo_sample(left-value.left, right-value.right);
    }
    inline stereo_sample<T> operator+(const T &value) {
        return stereo_sample(left+value, right+value);
    }
    inline stereo_sample<T> operator-(const T &value) {
        return stereo_sample(left-value, right-value);
    }
    inline stereo_sample<float> operator+(float value) {
        return stereo_sample<float>(left+value, right+value);
    }
    inline stereo_sample<float> operator-(float value) {
        return stereo_sample<float>(left-value, right-value);
    }
    inline stereo_sample<double> operator+(double value) {
        return stereo_sample<double>(left+value, right+value);
    }
    inline stereo_sample<double> operator-(double value) {
        return stereo_sample<double>(left-value, right-value);
    }
};

template<class T>
inline stereo_sample<T> operator*(const T &value, const stereo_sample<T> &value2) {
    return stereo_sample<T>(value2.left*value, value2.right*value);
}

template<class T>
inline stereo_sample<T> operator+(const T &value, const stereo_sample<T> &value2) {
    return stereo_sample<T>(value2.left+value, value2.right+value);
}

template<class T>
inline stereo_sample<T> operator-(const T &value, const stereo_sample<T> &value2) {
    return stereo_sample<T>(value-value2.left, value-value2.right);
}

template<typename T>
inline stereo_sample<T> shr(stereo_sample<T> v, int bits = 1) {
    v.left = shr(v.left, bits);
    v.right = shr(v.right, bits);
    return v;
}

template<typename T> 
inline void zero(stereo_sample<T> &v) {
    dsp::zero(v.left);
    dsp::zero(v.right);
}

template<typename T>
inline T small_value() {
    return 0;
}

template<>
inline float small_value<float>() {
    return (1.0/16777216.0); // allows for 2^-24, should be enough for 24-bit DACs at least :)
}

template<>
inline double small_value<double>() {
    return (1.0/16777216.0);
}

template<typename T> 
inline float mono(T v) {
    return v;
}

template<typename T> 
inline T mono(stereo_sample<T> v) {
    return shr(v.left+v.right);
}

template<typename T> 
inline T clip(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

inline double clip11(double value) {
    double a = fabs(value);
    if (a<=1) return value;
    return (a<0) ? -1.0 : 1.0;
}

inline float clip11(float value) {
    float a = fabsf(value);
    if (a<=1) return value;
    return (a<0) ? -1.0f : 1.0f;
}

inline double clip01(double value) {
    double a = fabs(value-0.5);
    if (a<=0.5) return value;
    return (a<0) ? -0.0 : 1.0;
}

inline float clip01(float value) {
    float a = fabsf(value-0.5f);
    if (a<=0.5f) return value;
    return (a<0) ? -0.0f : 1.0f;
}

template<typename T, typename U>
inline T lerp(T v1, T v2, U mix) {
    return v1+(v2-v1)*mix;
}

template<typename T>
inline stereo_sample<T> lerp(stereo_sample<T> &v1, stereo_sample<T> &v2, float mix) {
    return stereo_sample<T>(v1.left+(v2.left-v1.left)*mix, v1.right+(v2.right-v1.right)*mix);
}

/**
 * decay-only envelope (linear or exponential); deactivates itself when it goes below a set point (epsilon)
 */
template<typename T>
class decay
{
    T value, zero;
    bool active;
public:
    decay() {
        active = false;
        dsp::zero(zero);
    }
    inline bool get_active() {
        return active;
    }
    inline T get() {
        return active ? value : zero;
    }
    inline void set(const T &v) {
        value = v;
        active = true;
    }
    inline void add(const T &v) {
        if (active)
            value += v;
        else
            value = v;
        active = true;
    }
    template<typename U>inline void age_exp(U constant, U epsilon) {
        if (active) {
            value *= (1.0-constant);
            if (value < epsilon)
                active = false;
        }
    }
    template<typename U>inline void age_lin(U constant, U epsilon) {
        if (active) {
            value -= constant;
            if (value < epsilon)
                active = false;
        }
    }
};

class scheduler;

class task {
public:
    virtual void execute(scheduler *s)=0;
    virtual void dispose() { delete this; }
    virtual ~task() {}
};

/// this scheduler is based on std::multimap, so it isn't very fast, I guess
/// maybe some day it should be rewritten to use heapsort or something
/// work in progress, don't use!
class scheduler {
    std::multimap<unsigned int, task *> timeline;
    unsigned int time, next_task;
    bool eob;
    class end_buf_task: public task {
    public:
        scheduler *p;
        end_buf_task(scheduler *_p) : p(_p) {}
        virtual void execute(scheduler *s) { p->eob = true; }
        virtual void dispose() { }
    } eobt;
public:

    scheduler()
    : time(0)
    , next_task((unsigned)-1)
    , eob(true)
    , eobt (this)
    {
        time = 0;
        next_task = (unsigned)-1;
        eob = false;
    }
    inline bool is_next_tick() {
        if (time < next_task)
            return true;
        do_tasks();
    }
    inline void next_tick() {
        time++;
    }
    void set(int pos, task *t) {
        timeline.insert(pair<unsigned int, task *>(time+pos, t));
        next_task = timeline.begin()->first;
    }
    void do_tasks() {
        std::multimap<unsigned int, task *>::iterator i = timeline.begin();
        while(i != timeline.end() && i->first == time) {
            i->second->execute(this);
            i->second->dispose();
            timeline.erase(i);
        }
    }
    bool is_eob() {
        return eob;
    }
    void set_buffer_size(int count) {
        set(count, &eobt);
    }
};

/**
 * Force "small enough" float value to zero
 */
inline void sanitize(float &value)
{
    if (std::abs(value) < small_value<float>())
        value = 0.f;
}

/**
 * Force "small enough" double value to zero
 */
inline void sanitize(double &value)
{
    if (std::abs(value) < small_value<double>())
        value = 0.f;
}

/**
 * Force "small enough" stereo value to zero
 */
template<class T>
inline void sanitize(stereo_sample<T> &value)
{
    sanitize(value.left);
    sanitize(value.right);
}

inline float fract16(unsigned int value)
{
    return (value & 0xFFFF) * (1.0 / 65536.0);
}

/**
 * typical precalculated sine table
 */
template<class T, int N, int Multiplier>
class sine_table
{
public:
    static bool initialized;
    static T data[N+1];
    sine_table() {
        if (initialized)
            return;
        initialized = true;
        for (int i=0; i<N+1; i++)
            data[i] = (T)(Multiplier*sin(i*2*M_PI*(1.0/N)));
    }
};

template<class T, int N, int Multiplier>
bool sine_table<T,N,Multiplier>::initialized = false;

template<class T, int N, int Multiplier>
T sine_table<T,N,Multiplier>::data[N+1];

};

#endif