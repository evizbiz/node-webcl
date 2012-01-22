/*
** This file contains proprietary software owned by Motorola Mobility, Inc. **
** No rights, expressed or implied, whatsoever to this software are provided by Motorola Mobility, Inc. hereunder. **
**
** (c) Copyright 2011 Motorola Mobility, Inc.  All Rights Reserved.  **
*/

#include "context.h"
#include "device.h"
#include "commandqueue.h"
#include "event.h"
#include "platform.h"
#include "memoryobject.h"
#include "program.h"
#include "sampler.h"

#include <node_buffer.h>
#include <GL/gl.h>

#include <iostream>
#include <vector>
using namespace std;

using namespace node;
using namespace v8;

namespace webcl {

Persistent<FunctionTemplate> Context::constructor_template;

void Context::Init(Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Context::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);

  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Context"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_getContextInfo", getContextInfo);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createProgram", createProgram);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createCommandQueue", createCommandQueue);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createBuffer", createBuffer);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createImage2D", createImage2D);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createImage3D", createImage3D);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createSampler", createSampler);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_createUserEvent", createUserEvent);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "_getSupportedImageFormats", getSupportedImageFormats);

  target->Set(String::NewSymbol("Context"), constructor_template->GetFunction());
}

Context::Context(Handle<Object> wrapper) : context(0)
{
}

void Context::Destructor()
{
  cout<<"  Destroying CL context"<<endl;
  if(context) ::clReleaseContext(context);
  context=0;
}

JS_METHOD(Context::getContextInfo)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_context_info param_name = args[0]->Uint32Value();

  switch (param_name) {
  case CL_CONTEXT_REFERENCE_COUNT:
  case CL_CONTEXT_NUM_DEVICES: {
    cl_uint param_value=0;
    cl_int ret=::clGetContextInfo(context->getContext(),param_name,sizeof(cl_uint), &param_value, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(CL_INVALID_CONTEXT);
      REQ_ERROR_THROW(CL_INVALID_VALUE);
      REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
      REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
      return ThrowError("UNKNOWN ERROR");
    }
    return scope.Close(JS_INT(param_value));
  }
  case CL_CONTEXT_DEVICES: {
    size_t n=0;
    cl_int ret=::clGetContextInfo(context->getContext(),param_name,0,NULL, &n);
    cout<<"Found "<<n<<" devices"<<endl;

    cl_device_id ctx[n];
    ret=::clGetContextInfo(context->getContext(),param_name,sizeof(cl_device_id)*n, ctx, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(CL_INVALID_CONTEXT);
      REQ_ERROR_THROW(CL_INVALID_VALUE);
      REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
      REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
      return ThrowError("UNKNOWN ERROR");
    }

    Local<Array> arr = Array::New(n);
    for(int i=0;i<n;i++) {
      cout<<"Returning device "<<i<<": "<<ctx[i]<<endl;
      arr->Set(i,Device::New(ctx[i])->handle_);
    }
    return scope.Close(arr);
  }
  case CL_CONTEXT_PROPERTIES: {
    size_t n=0;
    cl_int ret=::clGetContextInfo(context->getContext(),param_name,0,NULL, &n);
    cl_context_properties ctx[n];
    ret=::clGetContextInfo(context->getContext(),param_name,sizeof(cl_context_properties)*n, ctx, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(CL_INVALID_CONTEXT);
      REQ_ERROR_THROW(CL_INVALID_VALUE);
      REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
      REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
      return ThrowError("UNKNOWN ERROR");
    }

    Local<Array> arr = Array::New(n);
    for(int i=0;i<n;i++) {
      arr->Set(i,JS_INT(ctx[i]));
    }
    return scope.Close(arr);
  }
  default:
    return ThrowError("UNKNOWN param_name");
  }
}

JS_METHOD(Context::createProgram)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_program pw=NULL;
  cl_int ret = CL_SUCCESS;

  // either we pass a code (string) or binary buffers
  if(args[0]->IsString()) {
    Local<String> str = args[0]->ToString();
    String::AsciiValue astr(str);

    size_t lengths[]={astr.length()};
    const char *strings[]={*astr};
    //cout<<"Creating program with source: "<<lengths[0]<<" bytes, \n"<<strings[0]<<endl;
    pw=::clCreateProgramWithSource(context->getContext(), 1, strings, lengths, &ret);

    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(CL_INVALID_CONTEXT);
      REQ_ERROR_THROW(CL_INVALID_VALUE);
      REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
      REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
      return ThrowError("UNKNOWN ERROR");
    }
    return scope.Close(Program::New(pw)->handle_);
  }
  else if(args[0]->IsArray()){
    Local<Array> devArray = Array::Cast(*args[0]);
    const size_t num=devArray->Length();
    vector<cl_device_id> devices;

    for (size_t i=0; i<num; i++) {
      Device *device = ObjectWrap::Unwrap<Device>(devArray->Get(i)->ToObject());
      devices.push_back(device->getDevice());
    }

    Local<Array> binArray = Array::Cast(*args[1]);
    const ::size_t n = binArray->Length();
    ::size_t* lengths = new size_t[n];
    const unsigned char** images =  new const unsigned char*[n];

    for (::size_t i = 0; i < n; ++i) {
      Local<Object> obj=binArray->Get(i)->ToObject();
        images[i] = (const unsigned char*) obj->GetIndexedPropertiesExternalArrayData();
        lengths[i] = obj->GetIndexedPropertiesExternalArrayDataLength();
    }

    pw=::clCreateProgramWithBinary(
                context->getContext(), (cl_uint) devices.size(),
                &devices.front(),
                lengths, images,
                NULL, &ret);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(CL_INVALID_CONTEXT);
      REQ_ERROR_THROW(CL_INVALID_VALUE);
      REQ_ERROR_THROW(CL_INVALID_DEVICE);
      REQ_ERROR_THROW(CL_INVALID_BINARY);
      REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
      REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
      return ThrowError("UNKNOWN ERROR");
    }

    // TODO should we return binaryStatus?
    return scope.Close(Program::New(pw)->handle_);
  }

  return Undefined();
}

JS_METHOD(Context::createCommandQueue)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_device_id device = ObjectWrap::Unwrap<Device>(args[0]->ToObject())->getDevice();
  cl_command_queue_properties properties = args[1]->NumberValue();

  cl_int ret=CL_SUCCESS;
  cl_command_queue cw = ::clCreateCommandQueue(context->getContext(), device, properties, &ret);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_CONTEXT);
    REQ_ERROR_THROW(CL_INVALID_DEVICE);
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    REQ_ERROR_THROW(CL_INVALID_QUEUE_PROPERTIES);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  return scope.Close(CommandQueue::New(cw)->handle_);
}

JS_METHOD(Context::createBuffer)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_mem_flags flags = args[0]->Uint32Value();
  size_t size = args[1]->Uint32Value();
  void *host_ptr = NULL;
  if(!args[2]->IsUndefined()) {
    if(args[2]->IsArray()) {
      Local<Array> arr=Array::Cast(*args[2]);
      host_ptr=arr->GetIndexedPropertiesExternalArrayData();
    }
    else if(args[2]->IsObject())
      host_ptr=args[2]->ToObject()->GetIndexedPropertiesExternalArrayData();
    else
      ThrowError("Invalid memory object");
  }

  cl_int ret=CL_SUCCESS;
  cl_mem mw = ::clCreateBuffer(context->getContext(), flags, size, host_ptr, &ret);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    REQ_ERROR_THROW(CL_INVALID_BUFFER_SIZE);
    REQ_ERROR_THROW(CL_INVALID_HOST_PTR);
    REQ_ERROR_THROW(CL_MEM_OBJECT_ALLOCATION_FAILURE);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  return scope.Close(MemoryObject::New(mw)->handle_);
}

JS_METHOD(Context::createImage2D)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_mem_flags flags = args[0]->NumberValue();

  cl_image_format image_format;
  Local<Object> obj = args[1]->ToObject();
  image_format.image_channel_order = obj->Get(JS_STR("order"))->Uint32Value();
  image_format.image_channel_data_type = obj->Get(JS_STR("data_type"))->Uint32Value();

  size_t width = args[2]->NumberValue();
  size_t height = args[3]->NumberValue();
  size_t row_pitch = args[4]->NumberValue();
  void *host_ptr=NULL;

  if(!args[5]->IsUndefined()) {
    host_ptr = args[5]->ToObject()->GetIndexedPropertiesExternalArrayData();
    //cout<<"Creating image: { order: "<<image_format.image_channel_order<<", datatype: "<<image_format.image_channel_data_type<<"} "
    //    <<"dim: "<<width<<"x"<<height<<" pitch "<<row_pitch<<endl;
  }
  cl_int ret=CL_SUCCESS;
  cl_mem mw = ::clCreateImage2D(
      context->getContext(), flags,&image_format,
      width, height, row_pitch, host_ptr, &ret);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_CONTEXT);
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    REQ_ERROR_THROW(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
    REQ_ERROR_THROW(CL_INVALID_IMAGE_SIZE);
    REQ_ERROR_THROW(CL_INVALID_HOST_PTR);
    REQ_ERROR_THROW(CL_IMAGE_FORMAT_NOT_SUPPORTED);
    REQ_ERROR_THROW(CL_MEM_OBJECT_ALLOCATION_FAILURE);
    REQ_ERROR_THROW(CL_INVALID_OPERATION);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  cout<<"Created image2d "<<hex<<mw<<dec<<endl;
  return scope.Close(MemoryObject::New(mw)->handle_);
}

JS_METHOD(Context::createImage3D)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_mem_flags flags = args[0]->NumberValue();

  cl_image_format image_format;
  Local<Object> obj = args[1]->ToObject();
  image_format.image_channel_order = obj->Get(JS_STR("order"))->Uint32Value();
  image_format.image_channel_data_type = obj->Get(JS_STR("data_type"))->Uint32Value();

  size_t width = args[2]->NumberValue();
  size_t height = args[3]->NumberValue();
  size_t depth = args[4]->NumberValue();
  size_t row_pitch = args[5]->NumberValue();
  size_t slice_pitch = args[6]->NumberValue();
  void *host_ptr=args[7]->IsUndefined() ? NULL : args[7]->ToObject()->GetIndexedPropertiesExternalArrayData();

  cl_int ret=CL_SUCCESS;
  cl_mem mw = ::clCreateImage3D(
              context->getContext(), flags, &image_format,
              width, height, depth, row_pitch,
              slice_pitch, host_ptr, &ret);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_CONTEXT);
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    REQ_ERROR_THROW(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
    REQ_ERROR_THROW(CL_INVALID_IMAGE_SIZE);
    REQ_ERROR_THROW(CL_INVALID_HOST_PTR);
    REQ_ERROR_THROW(CL_IMAGE_FORMAT_NOT_SUPPORTED);
    REQ_ERROR_THROW(CL_MEM_OBJECT_ALLOCATION_FAILURE);
    REQ_ERROR_THROW(CL_INVALID_OPERATION);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  return scope.Close(MemoryObject::New(mw)->handle_);
}

JS_METHOD(Context::createSampler)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_bool normalized_coords = args[0]->BooleanValue() ? CL_TRUE : CL_FALSE;
  cl_addressing_mode addressing_mode = args[1]->NumberValue();
  cl_filter_mode filter_mode = args[2]->NumberValue();

  cl_int ret=CL_SUCCESS;
  cl_sampler sw = ::clCreateSampler(
              context->getContext(),
              normalized_coords,
              addressing_mode,
              filter_mode,
              &ret);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_CONTEXT);
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    REQ_ERROR_THROW(CL_INVALID_OPERATION);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  return scope.Close(Sampler::New(sw)->handle_);
}

JS_METHOD(Context::getSupportedImageFormats)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_mem_flags flags = args[0]->NumberValue();
  cl_mem_object_type image_type = args[1]->NumberValue();
  cl_uint numEntries=0;

  cl_int ret = ::clGetSupportedImageFormats(
             context->getContext(),
             flags,
             image_type,
             0,
             NULL,
             &numEntries);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    return ThrowError("UNKNOWN ERROR");
  }

  cl_image_format* image_formats = new cl_image_format[numEntries];
  ret = ::clGetSupportedImageFormats(
      context->getContext(),
      flags,
      image_type,
      numEntries,
      image_formats,
      NULL);

  if (ret != CL_SUCCESS) {
    delete[] image_formats;
    REQ_ERROR_THROW(CL_INVALID_CONTEXT);
    REQ_ERROR_THROW(CL_INVALID_VALUE);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  Local<Array> imageFormats = Array::New();
  for (int i=0; i<numEntries; i++) {
    Local<Object> format = Object::New();
    format->Set(JS_STR("order"), JS_INT(image_formats[i].image_channel_order));
    format->Set(JS_STR("data_type"), JS_INT(image_formats[i].image_channel_data_type));
    imageFormats->Set(i, format);
  }
  delete[] image_formats;
  return scope.Close(imageFormats);
}

JS_METHOD(Context::createUserEvent)
{
  HandleScope scope;
  Context *context = UnwrapThis<Context>(args);
  cl_int ret=CL_SUCCESS;

  cl_event ew=::clCreateUserEvent(context->getContext(),&ret);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(CL_INVALID_CONTEXT);
    REQ_ERROR_THROW(CL_OUT_OF_RESOURCES);
    REQ_ERROR_THROW(CL_OUT_OF_HOST_MEMORY);
    return ThrowError("UNKNOWN ERROR");
  }

  return scope.Close(Event::New(ew)->handle_);
}

JS_METHOD(Context::New)
{
  if (!args.IsConstructCall())
    return ThrowTypeError("Constructor cannot be called as a function.");

  HandleScope scope;
  Context *cl = new Context(args.This());
  cl->Wrap(args.This());
  registerCLObj(cl);
  return scope.Close(args.This());
}

Context *Context::New(cl_context cw)
{

  HandleScope scope;

  Local<Value> arg = Integer::NewFromUnsigned(0);
  Local<Object> obj = constructor_template->GetFunction()->NewInstance(1, &arg);

  Context *context = ObjectWrap::Unwrap<Context>(obj);
  context->context = cw;

  return context;
}

}
