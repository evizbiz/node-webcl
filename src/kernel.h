// Copyright (c) 2011-2012, Motorola Mobility, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the Motorola Mobility, Inc. nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef KERNEL_H_
#define KERNEL_H_

#include "common.h"

namespace webcl {

class Kernel : public WebCLObject
{

public:
  void Destructor();

  static void Init(v8::Handle<v8::Object> exports);

  static Kernel *New(cl_kernel kw, WebCLObject *parent);
  static NAN_METHOD(New);
  static NAN_METHOD(getInfo);
  static NAN_METHOD(getWorkGroupInfo);
  static NAN_METHOD(getArgInfo);
  static NAN_METHOD(setArg);
  static NAN_METHOD(release);

  cl_kernel getKernel() const { return kernel; };

  virtual bool operator==(void *clObj) { return ((cl_kernel)clObj)==kernel; }

private:
  Kernel(v8::Handle<v8::Object> wrapper);
  ~Kernel();

  static v8::Persistent<v8::Function> constructor;

  cl_kernel kernel;

private:
  DISABLE_COPY(Kernel)
};

} // namespace

#endif
