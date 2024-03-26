#include "SkiaOpenGLHelper.h"
#include <SkiaOpenGLSurfaceFactory.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"

#pragma clang diagnostic pop

namespace RNSkia {

thread_local SkiaOpenGLContext ThreadContextHolder::ThreadSkiaOpenGLContext;

sk_sp<SkImage>
SkiaOpenGLSurfaceFactory::makeImageFromTexture(const SkImageInfo &info,
                                               const void *buffer) {
  // Setup OpenGL and Skia:
  auto ctx = ThreadContextHolder::ThreadSkiaOpenGLContext;
  if (!SkiaOpenGLHelper::createSkiaDirectContextIfNecessary(&ctx)) {

    RNSkLogger::logToConsole(
        "Could not create Skia Surface from native window / surface. "
        "Failed creating Skia Direct Context");
    return nullptr;
  }

  // TODO: do this only once:
  if (!ctx.interface->hasExtension("EGL_KHR_image") ||
      !ctx.interface->hasExtension("EGL_ANDROID_get_native_client_buffer") ||
      !ctx.interface->hasExtension("GL_OES_EGL_image_external") ||
      !ctx.interface->hasExtension("GL_OES_EGL_image") ||
      !ctx.interface->hasExtension("EGL_KHR_fence_sync") ||
      !ctx.interface->hasExtension("EGL_ANDROID_native_fence_sync")) {
    RNSkLogger::logToConsole(
        "Didn't find the right extensions to make a texture from a buffer");
    return nullptr;
  }

  fEGLGetNativeClientBufferANDROID =
      (EGLGetNativeClientBufferANDROIDProc)eglGetProcAddress(
          "eglGetNativeClientBufferANDROID");
  if (!fEGLGetNativeClientBufferANDROID) {
    RNSkLogger::logToConsole(
        "Failed to get the eglGetNativeClientBufferAndroid proc");
    return nullptr;
  }

  fEGLCreateImageKHR =
      (EGLCreateImageKHRProc)eglGetProcAddress("eglCreateImageKHR");
  if (!fEGLCreateImageKHR) {
    RNSkLogger::logToConsole("Failed to get the proc eglCreateImageKHR");
    return nullptr;
  }

  fEGLImageTargetTexture2DOES =
      (EGLImageTargetTexture2DOESProc)eglGetProcAddress(
          "glEGLImageTargetTexture2DOES");
  if (!fEGLImageTargetTexture2DOES) {
    RNSkLogger::logToConsole(
        "Failed to get the proc EGLImageTargetTexture2DOES");
    return nullptr;
  }

  fEGLCreateSyncKHR =
      (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
  if (!fEGLCreateSyncKHR) {
    RNSkLogger::logToConsole("Failed to get the proc eglCreateSyncKHR");
    return nullptr;
  }
  fEGLWaitSyncKHR = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
  if (!fEGLWaitSyncKHR) {
    RNSkLogger::logToConsole("Failed to get the proc eglWaitSyncKHR");
    return nullptr;
  }
  fEGLGetSyncAttribKHR =
      (PFNEGLGETSYNCATTRIBKHRPROC)eglGetProcAddress("eglGetSyncAttribKHR");
  if (!fEGLGetSyncAttribKHR) {
    RNSkLogger::logToConsole("Failed to get the proc eglGetSyncAttribKHR");
    return nullptr;
  }
  fEGLDupNativeFenceFDANDROID =
      (PFNEGLDUPNATIVEFENCEFDANDROIDPROC)eglGetProcAddress(
          "eglDupNativeFenceFDANDROID");
  if (!fEGLDupNativeFenceFDANDROID) {
    RNSkLogger::logToConsole(
        "Failed to get the proc eglDupNativeFenceFDANDROID");
    return nullptr;
  }
  fEGLDestroySyncKHR =
      (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
  if (!fEGLDestroySyncKHR) {
    RNSkLogger::logToConsole("Failed to get the proc eglDestroySyncKHR");
    return nullptr;
  }

  return nullptr;
}

sk_sp<SkSurface> SkiaOpenGLSurfaceFactory::makeOffscreenSurface(int width,
                                                                int height) {
  // Setup OpenGL and Skia:
  if (!SkiaOpenGLHelper::createSkiaDirectContextIfNecessary(
          &ThreadContextHolder::ThreadSkiaOpenGLContext)) {

    RNSkLogger::logToConsole(
        "Could not create Skia Surface from native window / surface. "
        "Failed creating Skia Direct Context");
    return nullptr;
  }

  auto colorType = kN32_SkColorType;

  SkSurfaceProps props(0, kUnknown_SkPixelGeometry);

  if (!SkiaOpenGLHelper::makeCurrent(
          &ThreadContextHolder::ThreadSkiaOpenGLContext,
          ThreadContextHolder::ThreadSkiaOpenGLContext.gl1x1Surface)) {
    RNSkLogger::logToConsole(
        "Could not create EGL Surface from native window / surface. Could "
        "not set new surface as current surface.");
    return nullptr;
  }

  // Create texture
  auto texture =
      ThreadContextHolder::ThreadSkiaOpenGLContext.directContext
          ->createBackendTexture(width, height, colorType,
                                 skgpu::Mipmapped::kNo, GrRenderable::kYes);

  if (!texture.isValid()) {
    RNSkLogger::logToConsole("couldn't create offscreen texture %dx%d", width,
                             height);
  }

  struct ReleaseContext {
    SkiaOpenGLContext *context;
    GrBackendTexture texture;
  };

  auto releaseCtx = new ReleaseContext(
      {&ThreadContextHolder::ThreadSkiaOpenGLContext, texture});

  // Create a SkSurface from the GrBackendTexture
  return SkSurfaces::WrapBackendTexture(
      ThreadContextHolder::ThreadSkiaOpenGLContext.directContext.get(), texture,
      kTopLeft_GrSurfaceOrigin, 0, colorType, nullptr, &props,
      [](void *addr) {
        auto releaseCtx = reinterpret_cast<ReleaseContext *>(addr);

        releaseCtx->context->directContext->deleteBackendTexture(
            releaseCtx->texture);
      },
      releaseCtx);
}

sk_sp<SkSurface> WindowSurfaceHolder::getSurface() {
  if (_skSurface == nullptr) {

    // Setup OpenGL and Skia
    if (!SkiaOpenGLHelper::createSkiaDirectContextIfNecessary(
            &ThreadContextHolder::ThreadSkiaOpenGLContext)) {
      RNSkLogger::logToConsole(
          "Could not create Skia Surface from native window / surface. "
          "Failed creating Skia Direct Context");
      return nullptr;
    }

    // Now we can create a surface
    _glSurface = SkiaOpenGLHelper::createWindowedSurface(_window);
    if (_glSurface == EGL_NO_SURFACE) {
      RNSkLogger::logToConsole(
          "Could not create EGL Surface from native window / surface.");
      return nullptr;
    }

    // Now make this one current
    if (!SkiaOpenGLHelper::makeCurrent(
            &ThreadContextHolder::ThreadSkiaOpenGLContext, _glSurface)) {
      RNSkLogger::logToConsole(
          "Could not create EGL Surface from native window / surface. Could "
          "not set new surface as current surface.");
      return nullptr;
    }

    // Set up parameters for the render target so that it
    // matches the underlying OpenGL context.
    GrGLFramebufferInfo fboInfo;

    // We pass 0 as the framebuffer id, since the
    // underlying Skia GrGlGpu will read this when wrapping the context in the
    // render target and the GrGlGpu object.
    fboInfo.fFBOID = 0;
    fboInfo.fFormat = 0x8058; // GL_RGBA8

    GLint stencil;
    glGetIntegerv(GL_STENCIL_BITS, &stencil);

    GLint samples;
    glGetIntegerv(GL_SAMPLES, &samples);

    auto colorType = kN32_SkColorType;

    auto maxSamples =
        ThreadContextHolder::ThreadSkiaOpenGLContext.directContext
            ->maxSurfaceSampleCountForColorType(colorType);

    if (samples > maxSamples) {
      samples = maxSamples;
    }

    auto renderTarget = GrBackendRenderTargets::MakeGL(_width, _height, samples,
                                                       stencil, fboInfo);

    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);

    struct ReleaseContext {
      EGLSurface glSurface;
    };

    auto releaseCtx = new ReleaseContext({_glSurface});

    // Create surface object
    _skSurface = SkSurfaces::WrapBackendRenderTarget(
        ThreadContextHolder::ThreadSkiaOpenGLContext.directContext.get(),
        renderTarget, kBottomLeft_GrSurfaceOrigin, colorType, nullptr, &props,
        [](void *addr) {
          auto releaseCtx = reinterpret_cast<ReleaseContext *>(addr);
          SkiaOpenGLHelper::destroySurface(releaseCtx->glSurface);
          delete releaseCtx;
        },
        reinterpret_cast<void *>(releaseCtx));
  }

  return _skSurface;
}

} // namespace RNSkia