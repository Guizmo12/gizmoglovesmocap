#ifndef ByteBuffer_h
#define ByteBuffer_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

class ByteBuffer
{
public:
    ByteBuffer() : data(nullptr), capacity(0), position(0), length(0) {}

    void init(unsigned int buf_size) {
        if (data) delete[] data;
        data = new byte[buf_size];
        capacity = buf_size;
        clear();
    }

    void clear() {
        position = 0;
        length = 0;
    }

    void deAllocate() {
        if (data) {
            delete[] data;
            data = nullptr;
        }
        capacity = 0;
        position = 0;
        length = 0;
    }

    int getSize() {
        return capacity - length;
    }

    int getCapacity() {
        return capacity;
    }

    byte peek(unsigned int index) {
        if (index < length) {
            return data[(position + index) % capacity];
        }
        return 0; // or some error value
    }

    int putInFront(byte in) {
        if (length >= capacity) return -1; // buffer is full
        position = (position - 1 + capacity) % capacity;
        data[position] = in;
        length++;
        return 0;
    }

    int put(byte in) {
        if (length >= capacity) return -1; // buffer is full
        data[(position + length) % capacity] = in;
        length++;
        return 0;
    }

    int putIntInFront(int in) {
        return putInFront((byte)(in >> 24)) +
               putInFront((byte)(in >> 16)) +
               putInFront((byte)(in >> 8)) +
               putInFront((byte)in);
    }

    int putInt(int in) {
        return put((byte)(in >> 24)) +
               put((byte)(in >> 16)) +
               put((byte)(in >> 8)) +
               put((byte)in);
    }

    int putLongInFront(long in) {
        return putInFront((byte)(in >> 56)) +
               putInFront((byte)(in >> 48)) +
               putInFront((byte)(in >> 40)) +
               putInFront((byte)(in >> 32)) +
               putInFront((byte)(in >> 24)) +
               putInFront((byte)(in >> 16)) +
               putInFront((byte)(in >> 8)) +
               putInFront((byte)in);
    }

    int putLong(long in) {
        return put((byte)(in >> 56)) +
               put((byte)(in >> 48)) +
               put((byte)(in >> 40)) +
               put((byte)(in >> 32)) +
               put((byte)(in >> 24)) +
               put((byte)(in >> 16)) +
               put((byte)(in >> 8)) +
               put((byte)in);
    }

    int putFloatInFront(float in) {
        byte* p = (byte*)&in;
        return putInFront(p[3]) +
               putInFront(p[2]) +
               putInFront(p[1]) +
               putInFront(p[0]);
    }

    int putFloat(float in) {
        byte* p = (byte*)&in;
        return put(p[3]) +
               put(p[2]) +
               put(p[1]) +
               put(p[0]);
    }

    byte get() {
        if (length == 0) return 0; // or some error value
        byte value = data[position];
        position = (position + 1) % capacity;
        length--;
        return value;
    }

    byte getFromBack() {
        if (length == 0) return 0; // or some error value
        length--;
        return data[(position + length) % capacity];
    }

    int getInt() {
        return (get() << 24) +
               (get() << 16) +
               (get() << 8) +
               get();
    }

    int getIntFromBack() {
        return (getFromBack() << 24) +
               (getFromBack() << 16) +
               (getFromBack() << 8) +
               getFromBack();
    }

    long getLong() {
        return ((long)get() << 56) +
               ((long)get() << 48) +
               ((long)get() << 40) +
               ((long)get() << 32) +
               ((long)get() << 24) +
               ((long)get() << 16) +
               ((long)get() << 8) +
               get();
    }

    long getLongFromBack() {
        return ((long)getFromBack() << 56) +
               ((long)getFromBack() << 48) +
               ((long)getFromBack() << 40) +
               ((long)getFromBack() << 32) +
               ((long)getFromBack() << 24) +
               ((long)getFromBack() << 16) +
               ((long)getFromBack() << 8) +
               getFromBack();
    }

    float getFloat() {
        float value;
        byte* p = (byte*)&value;
        p[0] = get();
        p[1] = get();
        p[2] = get();
        p[3] = get();
        return value;
    }

    float getFloatFromBack() {
        float value;
        byte* p = (byte*)&value;
        p[3] = getFromBack();
        p[2] = getFromBack();
        p[1] = getFromBack();
        p[0] = getFromBack();
        return value;
    }

    const byte* array() {
        return data;
    }

    unsigned int size() {
        return length;
    }

private:
    byte* data;
    unsigned int capacity;
    unsigned int position;
    unsigned int length;
};

#endif
