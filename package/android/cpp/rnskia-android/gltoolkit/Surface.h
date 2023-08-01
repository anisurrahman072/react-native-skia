#pragma once

#include "gltoolkit/Macros.h"
#include "gltoolkit/Proc.h"

namespace RNSkia {

class Surface {
public:
  Surface(EGLDisplay display, EGLSurface surface);

  ~Surface();

  bool IsValid() const;

  const EGLSurface &GetHandle() const;

  bool Present() const;

private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLSurface surface_ = EGL_NO_SURFACE;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

} // namespace RNSkia