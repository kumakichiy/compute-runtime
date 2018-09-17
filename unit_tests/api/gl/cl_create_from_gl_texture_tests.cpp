/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"

using namespace OCLRT;

typedef api_tests clCreateFromGLTexture_;

namespace ULT {
TEST_F(clCreateFromGLTexture_, givenNullContextWhenCreateIsCalledThenErrorIsReturned) {
    int errCode = CL_SUCCESS;
    auto image = clCreateFromGLTexture(nullptr, CL_MEM_READ_WRITE, GL_TEXTURE_1D, 0, 0, &errCode);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(errCode, CL_INVALID_CONTEXT);
}
} // namespace ULT