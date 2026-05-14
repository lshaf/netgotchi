//
// MisoDcGuard — serialises access to the SD/TFT shared SPI bus on CoreS3,
// where SD MISO and TFT DC are wired to the same GPIO35.
//
// enter(): take mutex, re-route GPIO35 to SPI MISO, set INPUT.
// leave(): set GPIO35 OUTPUT HIGH (DC idle), release mutex.
//
// Used directly (Scope RAII) or indirectly via GuardedFsImpl, which wraps an
// fs::FS so every File operation is auto-guarded.
//

#pragma once

#include <FS.h>
#include <FSImpl.h>
#include <SPI.h>
#include <Arduino.h>
#include <esp32-hal-spi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class MisoDcGuard {
public:
  void init(SPIClass* spi, int8_t dcPin) {
    _spi   = spi;
    _dcPin = dcPin;
    if (_dcPin >= 0 && !_mutex) _mutex = xSemaphoreCreateRecursiveMutex();
  }

  void enter() {
    if (_dcPin < 0) return;
    xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    if (_spi && _spi->bus()) spiAttachMISO(_spi->bus(), _dcPin);
    gpio_set_direction((gpio_num_t)_dcPin, GPIO_MODE_INPUT);
  }

  void leave() {
    if (_dcPin < 0) return;
    gpio_set_direction((gpio_num_t)_dcPin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_dcPin, 1);
    xSemaphoreGiveRecursive(_mutex);
  }

  class Scope {
  public:
    Scope(MisoDcGuard& g) : _g(g) { _g.enter(); }
    ~Scope()                       { _g.leave(); }
  private:
    MisoDcGuard& _g;
  };

  bool active() const { return _dcPin >= 0; }

  void lock()   { if (_dcPin >= 0) xSemaphoreTakeRecursive(_mutex, portMAX_DELAY); }
  void unlock() { if (_dcPin >= 0) xSemaphoreGiveRecursive(_mutex); }

private:
  SPIClass*         _spi   = nullptr;
  int8_t            _dcPin = -1;
  SemaphoreHandle_t _mutex = nullptr;
};

// Accessor trick: fs::FS::_impl is protected. A subclass with no extra members
// can reinterpret-cast to expose it.
struct GuardedFsAccessor : public fs::FS {
  fs::FSImplPtr& impl() { return _impl; }
};

class GuardedFileImpl : public fs::FileImpl {
public:
  GuardedFileImpl(fs::FileImplPtr inner, MisoDcGuard* g) : _inner(inner), _g(g) {}

  size_t write(const uint8_t* buf, size_t size) override {
    if (!_inner) return 0;
    MisoDcGuard::Scope s(*_g);
    return _inner->write(buf, size);
  }
  size_t read(uint8_t* buf, size_t size) override {
    if (!_inner) return 0;
    MisoDcGuard::Scope s(*_g);
    return _inner->read(buf, size);
  }
  void flush() override {
    if (!_inner) return;
    MisoDcGuard::Scope s(*_g);
    _inner->flush();
  }
  bool seek(uint32_t pos, fs::SeekMode mode) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->seek(pos, mode);
  }
  size_t position() const override {
    if (!_inner) return 0;
    MisoDcGuard::Scope s(*_g);
    return _inner->position();
  }
  size_t size() const override {
    if (!_inner) return 0;
    MisoDcGuard::Scope s(*_g);
    return _inner->size();
  }
  bool setBufferSize(size_t size) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->setBufferSize(size);
  }
  void close() override {
    if (!_inner) return;
    MisoDcGuard::Scope s(*_g);
    _inner->close();
  }
  time_t getLastWrite() override {
    if (!_inner) return 0;
    MisoDcGuard::Scope s(*_g);
    return _inner->getLastWrite();
  }
  const char* path() const override { return _inner ? _inner->path() : ""; }
  const char* name() const override { return _inner ? _inner->name() : ""; }
  boolean isDirectory() override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->isDirectory();
  }
  fs::FileImplPtr openNextFile(const char* mode) override {
    if (!_inner) return fs::FileImplPtr();
    MisoDcGuard::Scope s(*_g);
    auto r = _inner->openNextFile(mode);
    return r ? std::make_shared<GuardedFileImpl>(r, _g) : r;
  }
  boolean seekDir(long position) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->seekDir(position);
  }
  String getNextFileName() override {
    if (!_inner) return String();
    MisoDcGuard::Scope s(*_g);
    return _inner->getNextFileName();
  }
  String getNextFileName(bool* isDir) override {
    if (!_inner) return String();
    MisoDcGuard::Scope s(*_g);
    return _inner->getNextFileName(isDir);
  }
  void rewindDirectory() override {
    if (!_inner) return;
    MisoDcGuard::Scope s(*_g);
    _inner->rewindDirectory();
  }
  operator bool() override { return _inner && _inner->operator bool(); }

private:
  fs::FileImplPtr _inner;
  MisoDcGuard*    _g;
};

class GuardedFsImpl : public fs::FSImpl {
public:
  GuardedFsImpl(fs::FSImplPtr inner, MisoDcGuard* g) : _inner(inner), _g(g) {}

  fs::FileImplPtr open(const char* path, const char* mode, const bool create) override {
    if (!_inner) return fs::FileImplPtr();
    MisoDcGuard::Scope s(*_g);
    auto r = _inner->open(path, mode, create);
    return r ? std::make_shared<GuardedFileImpl>(r, _g) : r;
  }
  bool exists(const char* path) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->exists(path);
  }
  bool rename(const char* from, const char* to) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->rename(from, to);
  }
  bool remove(const char* path) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->remove(path);
  }
  bool mkdir(const char* path) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->mkdir(path);
  }
  bool rmdir(const char* path) override {
    if (!_inner) return false;
    MisoDcGuard::Scope s(*_g);
    return _inner->rmdir(path);
  }

private:
  fs::FSImplPtr _inner;
  MisoDcGuard*  _g;
};

inline fs::FS makeGuardedFs(fs::FS& inner, MisoDcGuard& guard) {
  auto innerImpl = static_cast<GuardedFsAccessor&>(inner).impl();
  return fs::FS(std::make_shared<GuardedFsImpl>(innerImpl, &guard));
}
