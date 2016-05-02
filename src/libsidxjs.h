/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#ifndef LIBSIDXJS_H
#define LIBSIDXJS_H

#include <nan.h>
#include <v8.h>
#include <node.h>
extern "C" {
  #include <spatialindex/capi/sidx_api.h>
}

using namespace v8;
using namespace node;

class SpatialIndex : public Nan::ObjectWrap {
 public:
  static void Init(v8::Local<v8::Object> exports);
  static void Open(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Version(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Dimension(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void InsertData(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void DeleteData(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Intersects(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Bounds(const Nan::FunctionCallbackInfo<v8::Value>& info);
  void SetIndex(IndexH h){ handle = h;};
  IndexH GetIndex() const { return handle; };
  void SetProperties(IndexPropertyH p){ props = p; };
  IndexPropertyH GetProperties() const { return props; };
 private:
  explicit SpatialIndex();
  ~SpatialIndex();
  IndexH handle = NULL;
  IndexPropertyH props = NULL;

  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static Nan::Persistent<v8::Function> constructor;
};

#endif
