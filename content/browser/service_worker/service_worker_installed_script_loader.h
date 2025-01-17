// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_LOADER_H_

#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_installed_script_reader.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

// S13nServiceWorker: A URLLoader that loads an installed service worker script
// for a service worker that doesn't have a
// ServiceWorkerInstalledScriptsManager.
//
// Some cases where this happens:
// - a new (non-installed) service worker requests a script that it already
//   installed, e.g., importScripts('a.js') multiple times.
// - a service worker that was new when it started and became installed while
//   running requests an installed script, e.g., importScripts('a.js') after
//   installation.
class CONTENT_EXPORT ServiceWorkerInstalledScriptLoader
    : public network::mojom::URLLoader,
      public ServiceWorkerInstalledScriptReader::Client,
      public mojo::DataPipeDrainer::Client {
 public:
  ServiceWorkerInstalledScriptLoader(
      uint32_t options,
      network::mojom::URLLoaderClientPtr client,
      std::unique_ptr<ServiceWorkerResponseReader> response_reader);
  ~ServiceWorkerInstalledScriptLoader() override;

  // ServiceWorkerInstalledScriptReader::Client overrides:
  void OnStarted(std::string encoding,
                 base::flat_map<std::string, std::string> headers,
                 mojo::ScopedDataPipeConsumerHandle body_handle,
                 uint64_t body_size,
                 mojo::ScopedDataPipeConsumerHandle meta_data_handle,
                 uint64_t meta_data_size) override;
  void OnHttpInfoRead(
      scoped_refptr<HttpResponseInfoIOBuffer> http_info) override;
  void OnFinished(
      ServiceWorkerInstalledScriptReader::FinishedReason reason) override;

  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const base::Optional<std::vector<std::string>>&
          to_be_removed_request_headers,
      const base::Optional<net::HttpRequestHeaders>& modified_request_headers,
      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  // mojo::DataPipeDrainer::Client overrides:
  // These just do nothing.
  void OnDataAvailable(const void* data, size_t num_bytes) override {}
  void OnDataComplete() override {}

  uint32_t options_ = network::mojom::kURLLoadOptionNone;
  network::mojom::URLLoaderClientPtr client_;
  base::TimeTicks request_start_;
  std::unique_ptr<ServiceWorkerInstalledScriptReader> reader_;

  std::string encoding_;
  mojo::ScopedDataPipeConsumerHandle body_handle_;
  uint64_t body_size_ = 0;
  std::unique_ptr<mojo::DataPipeDrainer> metadata_drainer_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerInstalledScriptLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_LOADER_H_
