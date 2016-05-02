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
#include "libsidxjs.h"

Nan::Persistent<v8::Function> SpatialIndex::constructor;

constexpr
unsigned int hash(const char* str, int h = 0)
{
  return !str[h] ? 5381 : (hash(str, h+1)*33) ^ str[h];
}

const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

void toArray(Local<Array>& input, std::vector<double>& values) {
  unsigned int numValues = input->Length();
  for (unsigned int i = 0; i < numValues; i++) {
    values.push_back(input->Get(i)->NumberValue());
  }
}

class SIDXOpenWorker : public Nan::AsyncWorker {
public:
  SIDXOpenWorker(Nan::Callback *callback, SpatialIndex *idx) : Nan::AsyncWorker(callback) {
    this->sidx = idx;
  }
  ~SIDXOpenWorker() {}

  void Execute() {
    if (this->sidx->GetProperties() == NULL){
      // set basic default in-memory r*-tree
      IndexPropertyH props = IndexProperty_Create();
      IndexProperty_SetIndexType(props, RT_RTree);
      IndexProperty_SetIndexStorage(props, RT_Memory);
      // note we use local properties here as these will be owned
      // by the index
      this->sidx->SetIndex(Index_Create(props));
    } else {
      IndexH idx = Index_Create(sidx->GetProperties());
      if (idx == NULL){
        err = 1;
        char* pszErrMsg = Error_GetLastErrorMsg();
        this->errMsg = std::string(pszErrMsg);
        free(pszErrMsg);
      } else{
        this->sidx->SetIndex(idx);
      }
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    if (this->err) {
      std::string msg = "Error opening Index: " + this->errMsg;
      Local<Value> argv[] = {Exception::Error(Nan::New<String>(msg).ToLocalChecked())};
      callback->Call(1, argv);
    } else {
      Local<Value> argv[] = {Nan::Null(),  Nan::Undefined()};
      callback->Call(2, argv);
    }
  }
  int err = 0;
  std::string errMsg;
  SpatialIndex* sidx = NULL;
};

class SIDXIntersectsWorker : public Nan::AsyncWorker {
public:
  SIDXIntersectsWorker(Nan::Callback *callback, SpatialIndex *idx,
      double* mins, double* maxs, uint32_t dims, uint32_t offset, uint32_t len) : Nan::AsyncWorker(callback) {
    this->sidx = idx;
    this->mins.assign(mins, mins + dims);
    this->maxs.assign(maxs, maxs + dims);
    this->dims = dims;
    this->offset = offset;
    this->length = len;
  }
  ~SIDXIntersectsWorker() {}

  void Execute() {
    int64_t l = Index_GetResultSetLimit(this->sidx->GetIndex());
    int64_t o = Index_GetResultSetOffset(this->sidx->GetIndex());

    if (this->length > 0){
        Index_SetResultSetLimit(this->sidx->GetIndex(), this->length);
    }
    if (this->offset > 0){
        Index_SetResultSetOffset(this->sidx->GetIndex(), this->offset);
    }
    if (Index_Intersects_obj(this->sidx->GetIndex(), (double*)&(this->mins[0]), (double*)(&this->maxs[0]),
                          this->dims, &items, &nResults) != RT_None){
      char* pszErrMsg = Error_GetLastErrorMsg();
      errMsg = std::string(pszErrMsg);
      free(pszErrMsg);
      err = 1;
    }
    Index_SetResultSetLimit(this->sidx->GetIndex(), l);
    Index_SetResultSetOffset(this->sidx->GetIndex(), o);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    if (this->err) {
      std::string msg = "Error performing Intersects: " + this->errMsg;
      Local<Value> argv[] = {Exception::Error(Nan::New<String>(msg).ToLocalChecked())};
      callback->Call(1, argv);
    } else {
      v8::Local<v8::Array> results = v8::Local<v8::Array>(Nan::New<v8::Array>());
      for(uint64_t i = 0; i < nResults; i++) {
        unsigned char* pData = NULL;
        uint64_t len = 0;
        IndexItemH item = items[i];
        if (IndexItem_GetData(item, (uint8_t **)&pData, &len) == RT_None)
        {
          int64_t id = IndexItem_GetID(item);
          v8::Local<v8::Object> obj = Nan::New<v8::Object>();
          Nan::Set(obj, Nan::New<v8::String>("id").ToLocalChecked(),
            Nan::New<Number>(id));
          // ownership of pData has been transferred to the buffer for management
          Nan::Set(obj, Nan::New<v8::String>("data").ToLocalChecked(),
            Nan::NewBuffer(reinterpret_cast<char*>(pData), len).ToLocalChecked());
          Nan::Set(results, static_cast<uint32_t>(i), obj);
        } else {
          char* pszErrMsg = Error_GetLastErrorMsg();
          this->errMsg = std::string(pszErrMsg);
          free(pszErrMsg);
          err = 1;
        }
      }

      if (this->nResults > 0){
        Index_DestroyObjResults(this->items, this->nResults);
      }
      Local<Value> argv[] = {Nan::Null(),  results};
      callback->Call(2, argv);
    }
  }

  int err = 0;
  std::string errMsg;
  SpatialIndex* sidx = NULL;
  std::vector<double> mins;
  std::vector<double> maxs;
  uint32_t dims = 0;
  uint32_t offset = 0;
  uint32_t length = 0;
  IndexItemH* items;
  uint64_t nResults;
};

class SIDXInsertWorker : public Nan::AsyncWorker {
public:
  SIDXInsertWorker(Nan::Callback *callback, SpatialIndex *idx, int64_t id,
      double* mins, double* maxs, uint32_t dims, unsigned char* pData, size_t dataLength) : Nan::AsyncWorker(callback) {
    this->sidx = idx;
    this->id = id;
    this->mins.assign(mins, mins + dims);
    this->maxs.assign(maxs, maxs + dims);
    this->dims = dims;
    this->dataLength = dataLength;
    this->data.assign(pData, pData + dataLength);
  }
  ~SIDXInsertWorker() {
  }

  void Execute() {
    if (Index_InsertData(this->sidx->GetIndex(), this->id, (double*)&(this->mins[0]), (double*)&(this->maxs[0]),
        this->dims, (uint8_t *)&(this->data[0]), this->dataLength) != RT_None){
      char* pszErrMsg = Error_GetLastErrorMsg();
      errMsg = std::string(pszErrMsg);
      free(pszErrMsg);
      err = 1;
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    if (this->err) {
      std::string msg = "Error inserting data: " + this->errMsg;
      Local<Value> argv[] = {Exception::Error(Nan::New<String>(msg).ToLocalChecked())};
      callback->Call(1, argv);
    } else {
      Local<Value> argv[] = {Nan::Null(),  Nan::Undefined()};
      callback->Call(2, argv);
    }
  }
  int err = 0;
  std::string errMsg;
  SpatialIndex* sidx = NULL;
  int64_t id = 0;
  std::vector<double> mins;
  std::vector<double> maxs;
  std::vector<unsigned char> data;
  size_t dataLength = 0;
  uint32_t dims = 0;
};

class SIDXVersionWorker : public Nan::AsyncWorker {
public:
  SIDXVersionWorker(Nan::Callback *callback) : Nan::AsyncWorker(callback) {
  }
  ~SIDXVersionWorker() {}

  void Execute() {
    char* v= SIDX_Version();
    version = std::string(v);
    free(v);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    Local<Value> argv[] = {Nan::Null(),  Nan::New<String>(this->version).ToLocalChecked()};
    callback->Call(2, argv);
  }
  std::string version;
};

class SIDXBoundsWorker : public Nan::AsyncWorker {
public:
  SIDXBoundsWorker(Nan::Callback *callback, SpatialIndex *idx) : Nan::AsyncWorker(callback) {
    this->sidx = idx;
  }
  ~SIDXBoundsWorker() {
  }

  void Execute() {
    uint32_t dims;
    double* pMins;
    double* pMaxs;
    if (Index_GetBounds(this->sidx->GetIndex(), &pMins, &pMaxs, &dims) == RT_None){
      this->mins.assign(pMins, pMins + dims);
      this->maxs.assign(pMaxs, pMaxs + dims);
      this->dims = dims;
      free(pMins);
      free(pMaxs);
    } else {
      char* pszErrMsg = Error_GetLastErrorMsg();
      errMsg = std::string(pszErrMsg);
      free(pszErrMsg);
      this->err = 1;
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    if (this->err) {
      std::string msg = "Error gettings bounds: " + this->errMsg;
      Local<Value> argv[] = {Exception::Error(Nan::New<String>(msg).ToLocalChecked())};
      callback->Call(1, argv);
    } else {
      v8::Local<v8::Array> results = v8::Local<v8::Array>(Nan::New<v8::Array>());
      for(uint64_t i = 0; i < this->dims; i++) {
        Nan::Set(results, static_cast<uint32_t>(i), Nan::New<v8::Number>(this->mins[i]));
        Nan::Set(results, static_cast<uint32_t>(dims + i), Nan::New<v8::Number>(this->maxs[i]));
      }
      Local<Value> argv[] = {Nan::Null(),  results};
      callback->Call(2, argv);
    }
  }
  int err = 0;
  std::string errMsg;
  SpatialIndex* sidx = NULL;
  std::vector<double> mins;
  std::vector<double> maxs;
  uint32_t dims = 0;
};

class SIDXDeleteWorker : public Nan::AsyncWorker {
public:
  SIDXDeleteWorker(Nan::Callback *callback, SpatialIndex *idx, int64_t id,
      double* mins, double* maxs, uint32_t dims) : Nan::AsyncWorker(callback) {
    this->sidx = idx;
    this->id = id;
    this->mins.assign(mins, mins + dims);
    this->maxs.assign(maxs, maxs + dims);
    this->dims = dims;
  }
  ~SIDXDeleteWorker() {
  }

  void Execute() {
    if (Index_DeleteData(this->sidx->GetIndex(), this->id, (double*)&(this->mins[0]), (double*)&(this->maxs[0]),
        this->dims) != RT_None){
      char* pszErrMsg = Error_GetLastErrorMsg();
      errMsg = std::string(pszErrMsg);
      free(pszErrMsg);
      err = 1;
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    if (this->err) {
      std::string msg = "Error deleting data: " + this->errMsg;
      Local<Value> argv[] = {Exception::Error(Nan::New<String>(msg).ToLocalChecked())};
      callback->Call(1, argv);
    } else {
      Local<Value> argv[] = {Nan::Null(),  Nan::Undefined()};
      callback->Call(2, argv);
    }
  }
  int err = 0;
  std::string errMsg;
  SpatialIndex* sidx = NULL;
  int64_t id = 0;
  std::vector<double> mins;
  std::vector<double> maxs;
  uint32_t dims = 0;
};

SpatialIndex::SpatialIndex(){
}

SpatialIndex::~SpatialIndex() {
  if (handle != NULL) {
    Index_Destroy(handle);
    handle = NULL;
  }
  else if (props != NULL) {
    // props were created but handle wasn't which can happen if open fails
    IndexProperty_Destroy (props);
    props = NULL;
  }
}

void SpatialIndex::Init(v8::Local<v8::Object> exports) {
  Nan::HandleScope scope;

  // Prepare constructor template
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("SpatialIndex").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "open", Open);
  Nan::SetPrototypeMethod(tpl, "version", Version);
  Nan::SetPrototypeMethod(tpl, "dimension", Dimension);
  Nan::SetPrototypeMethod(tpl, "insert", InsertData);
  Nan::SetPrototypeMethod(tpl, "delete", DeleteData);
  Nan::SetPrototypeMethod(tpl, "intersects", Intersects);
  Nan::SetPrototypeMethod(tpl, "bounds", Bounds);
  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("SpatialIndex").ToLocalChecked(), tpl->GetFunction());
}

void SpatialIndex::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new SpatialIndex(...)`
    Handle<Array> propertyNames;
    IndexPropertyH props = NULL;
    v8::Local<v8::Value> params = info[0];

    if (params->IsObject()) {
      Handle<Object> jsParam;
      int i, n = 0;
      props = IndexProperty_Create();
      jsParam = Handle<Object>::Cast(params);
      propertyNames = jsParam->GetPropertyNames();
      n = propertyNames->Length();
      for (i = 0; i < n ; i++) {
        Handle<Value> b = propertyNames->Get(Nan::New<Integer>(i));
        const char* k = std::string(*String::Utf8Value(b)).c_str();
        const char* v = std::string(*String::Utf8Value(jsParam->Get(b))).c_str();
        switch (hash(k)) {
          case  hash("type"):
            switch(hash(v)) {
              case hash ("rtree"):
                IndexProperty_SetIndexType(props, RT_RTree);
                break;
              default:
                break;
            }
            break;
          case  hash("storage"):
            switch(hash(v)){
              case hash("memory"):
                IndexProperty_SetIndexStorage(props, RT_Memory);
                break;
              case hash("disk"): {
                Local<String> s = Nan::Get(jsParam, Nan::New<String>("filename").ToLocalChecked()).ToLocalChecked()->ToString();
                Nan::Utf8String val(s);
                std::string fname(*val);
                IndexProperty_SetFileName(props, fname.c_str());
                IndexProperty_SetIndexStorage(props, RT_Disk);
                break;
              }
              default:
                break;
            }
            break;
          case hash("variant"):
            switch(hash(v)){
              case hash("rstar"):
                IndexProperty_SetIndexVariant(props, RT_Star);
                break;
              case hash("linear"):
                IndexProperty_SetIndexVariant(props, RT_Linear);
                break;
              case hash("quadratic"):
                IndexProperty_SetIndexVariant(props, RT_Quadratic);
                break;
              default:
                break;
            }
            break;
          case hash("dimension"):
            switch(hash(v)){
                case hash("2"):
                  IndexProperty_SetDimension(props, 2);
                  break;
                case hash("3"):
                  IndexProperty_SetDimension(props, 3);
                  break;
                default:
                  break;
            }
          default:
            break;
        };
      }
    }
    SpatialIndex* obj = new SpatialIndex();
    obj->props = props;
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // Invoked as plain function `SpatialIndex(...)`, turn into construct call.
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = { info[0] };
    v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
    info.GetReturnValue().Set(cons->NewInstance(argc, argv));
  }
}

void SpatialIndex::Open(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() == 1){
    Nan::HandleScope scope;
    SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    AsyncQueueWorker(new SIDXOpenWorker(callback, index));
  } else {
    Nan::ThrowError("Open requires a callback function");
  }
}

void SpatialIndex::Version(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() == 1){
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    AsyncQueueWorker(new SIDXVersionWorker(callback));
  } else {
    Nan::ThrowError("Version requires a callback function");
  }
}

void SpatialIndex::Dimension(const Nan::FunctionCallbackInfo<v8::Value>& info){
  SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
  if (index->handle == NULL){
    Nan::ThrowError("Index must be open");
  } else {
    IndexPropertyH props = Index_GetProperties(index->handle);
    uint32_t dim = IndexProperty_GetDimension(props);
    info.GetReturnValue().Set(Nan::New<Uint32>(dim));
  }
}

void SpatialIndex::InsertData(const Nan::FunctionCallbackInfo<v8::Value>& info){
  SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
  if (index->handle == NULL){
    Nan::ThrowError("Index must be open");
  } else {
    // id, mins, maxs, data, cb, data is optional
    if ((info.Length() == 4) || (info.Length() == 5)){
      if ((info[0]->IsNumber()) && (info[1]->IsArray()) && (info[2]->IsArray())){
        int64_t id = 0;
        uint32_t dims = 0;
        unsigned char* pData = NULL;
        size_t dataLen = 0;
        std::vector<double> mins;
        std::vector<double> maxs;
        Nan::Callback *callback;

        if (info.Length() == 4){
          callback = new Nan::Callback(info[3].As<Function>());
        } else {
          callback = new Nan::Callback(info[4].As<Function>());
          Local<Object> bufferObj = info[3]->ToObject();
          pData = reinterpret_cast<unsigned char*>(Buffer::Data(bufferObj));
          dataLen = Buffer::Length(bufferObj);
        }

        id = info[0]->NumberValue();
        Local<Array> in1 = Local<Array>::Cast(info[1]);
        Local<Array> in2 = Local<Array>::Cast(info[2]);
        toArray(in1, mins);
        toArray(in2, maxs);
        dims = mins.size();

        Nan::HandleScope scope;
        SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
        AsyncQueueWorker(new SIDXInsertWorker(callback, index, id,
          (double*)&mins[0], (double*)&maxs[0], dims, pData, dataLen));
      } else {
        Nan::ThrowError("Insert requires numeric id, min and max MBR arrays");
      }
    } else {
      Nan::ThrowError("Insert requires numeric id, min and max");
    }
  }
}

void SpatialIndex::DeleteData(const Nan::FunctionCallbackInfo<v8::Value>& info){
  SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
  if (index->handle == NULL){
    Nan::ThrowError("Index must be open");
  } else {
    // id, mins, maxs, cb
    if ((info.Length() == 4) ){
      if ((info[0]->IsNumber()) && (info[1]->IsArray()) && (info[2]->IsArray())){
        int64_t id = 0;
        uint32_t dims = 0;
        std::vector<double> mins;
        std::vector<double> maxs;
        Nan::Callback *callback = new Nan::Callback(info[3].As<Function>());

        id = info[0]->NumberValue();
        Local<Array> in1 = Local<Array>::Cast(info[1]);
        Local<Array> in2 = Local<Array>::Cast(info[2]);
        toArray(in1, mins);
        toArray(in2, maxs);
        dims = mins.size();

        AsyncQueueWorker(new SIDXDeleteWorker(callback, index, id,
          (double*)&mins[0], (double*)&maxs[0], dims));
      } else {
        Nan::ThrowError("Insert requires numeric id, min and max MBR arrays");
      }
    } else {
      Nan::ThrowError("Insert requires numeric id, min and max");
    }
  }
}

void SpatialIndex::Intersects(const Nan::FunctionCallbackInfo<v8::Value>& info){
  SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
  if (index->handle == NULL){
    Nan::ThrowError("Index must be open");
  } else {
    // mins, maxs, cb
    // mins, maxs, offset, length, cb
    if ((info.Length() == 3) || (info.Length() == 5)){
      Nan::Callback *callback;
      uint32_t offset = 0;
      uint32_t length = 0;
      if (info.Length() == 3){
        callback = new Nan::Callback(info[2].As<Function>());
      } else {
        callback = new Nan::Callback(info[4].As<Function>());
        offset = info[2]->Uint32Value();
        length = info[3]->Uint32Value();
      }
      if ((info[0]->IsArray()) && (info[1]->IsArray())){
        uint32_t dims = 0;
        std::vector<double> mins;
        std::vector<double> maxs;

        Local<Array> in1 = Local<Array>::Cast(info[0]);
        Local<Array> in2 = Local<Array>::Cast(info[1]);
        toArray(in1, mins);
        toArray(in2, maxs);
        dims = mins.size();

        Nan::HandleScope scope;
        SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
        AsyncQueueWorker(new SIDXIntersectsWorker(callback, index,
          (double*)&mins[0], (double*)&maxs[0], dims, offset, length));
      } else {
        Nan::ThrowError("Intersect requires min and max MBR arrays, offset and length are optional");
      }
    } else {
      Nan::ThrowError("Intersect requires min and max MBR arrays offset and length are optional");
    }
  }
}

void SpatialIndex::Bounds(const Nan::FunctionCallbackInfo<v8::Value>& info){
  if (info.Length() == 1){
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    SpatialIndex* index = ObjectWrap::Unwrap<SpatialIndex>(info.Holder());
    if (index->handle == NULL){
      Nan::ThrowError("Index must be open");
    } else {
      AsyncQueueWorker(new SIDXBoundsWorker(callback, index));
    }
  } else{
    Nan::ThrowError("Bounds requires a callback function");
  }
}
