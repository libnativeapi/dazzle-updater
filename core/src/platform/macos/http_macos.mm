#include "../../foundation/http.h"

#import <Foundation/Foundation.h>

#include <fcntl.h>
#include <unistd.h>

#include <string>

namespace dazzle {

/// Calls the progress callback, if the caller installed one: an HttpProgress that is
/// empty (the release index fetch) must not be invoked.
bool ReportProgress(const HttpProgress* progress, int64_t received, int64_t total) {
  if (progress == nullptr || !*progress) {
    return true;
  }
  return (*progress)(received, total);
}

/// Shared between the calling thread and the NSURLSession delegate callbacks. The
/// delegate runs on the session's own queue, and the caller waits on `condition` until
/// `finished` is set, so the fields are only touched under that condition.
struct HttpTransferState {
  const HttpProgress* progress = nullptr;
  NSCondition* condition = nil;
  bool finished = false;
  bool cancelled = false;
  bool write_failed = false;
  int status = 0;
  int64_t received = 0;
  int64_t total = 0;
  int file = -1;
  NSMutableData* body = nil;
  std::string error;
  std::string error_code;
};

}  // namespace dazzle

@interface DazzleHttpDelegate : NSObject <NSURLSessionDataDelegate> {
 @public
  dazzle::HttpTransferState* state;
}
@end

@implementation DazzleHttpDelegate

- (void)URLSession:(NSURLSession*)session
                 dataTask:(NSURLSessionDataTask*)task
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
  if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
    state->status = (int)[(NSHTTPURLResponse*)response statusCode];
  }
  const int64_t expected = (int64_t)[response expectedContentLength];
  state->total = expected == (int64_t)NSURLResponseUnknownLength ? 0 : expected;
  if (!dazzle::ReportProgress(state->progress, 0, state->total)) {
    state->cancelled = true;
    completionHandler(NSURLSessionResponseCancel);
    return;
  }
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)task
    didReceiveData:(NSData*)data {
  const int64_t length = (int64_t)[data length];
  if (state->file >= 0) {
    const char* bytes = (const char*)[data bytes];
    int64_t written = 0;
    while (written < length) {
      const ssize_t count = write(state->file, bytes + written, (size_t)(length - written));
      if (count <= 0) {
        state->write_failed = true;
        [task cancel];
        return;
      }
      written += count;
    }
  } else if (state->body) {
    [state->body appendData:data];
  }
  state->received += length;
  if (!dazzle::ReportProgress(state->progress, state->received, state->total)) {
    state->cancelled = true;
    [task cancel];
  }
}

- (void)URLSession:(NSURLSession*)session
                 task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  if (error && !state->cancelled && !state->write_failed) {
    const char* description = [[error localizedDescription] UTF8String];
    state->error = description ? description : "the transfer failed";
    state->error_code = "network";
  }
  [state->condition lock];
  state->finished = true;
  [state->condition signal];
  [state->condition unlock];
}

@end

namespace dazzle {
namespace {

class MacosHttpImpl final : public HttpClient::Impl {
 public:
  HttpResult Get(const std::string& url, const std::string& destination_path,
                 const HttpProgress& progress) override {
    HttpResult result;
    @autoreleasepool {
      HttpTransferState state;
      state.progress = &progress;
      state.condition = [[[NSCondition alloc] init] autorelease];
      state.body = destination_path.empty() ? [NSMutableData data] : nil;
      if (!destination_path.empty()) {
        state.file = open(destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (state.file < 0) {
          result.error = "cannot write the download to " + destination_path;
          result.error_code = "permission";
          return result;
        }
      }

      NSURLSessionConfiguration* configuration =
          [NSURLSessionConfiguration ephemeralSessionConfiguration];
      configuration.timeoutIntervalForRequest = 30;
      configuration.timeoutIntervalForResource = 3600;
      DazzleHttpDelegate* delegate = [[[DazzleHttpDelegate alloc] init] autorelease];
      delegate->state = &state;
      NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration
                                                           delegate:delegate
                                                      delegateQueue:nil];
      NSMutableURLRequest* request =
          [NSMutableURLRequest requestWithURL:[NSURL URLWithString:
                                                  [NSString stringWithUTF8String:url.c_str()]]];
      // A cached release index or installer would defeat the point of checking.
      [request setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];
      NSURLSessionDataTask* task = [session dataTaskWithRequest:request];
      [task resume];

      [state.condition lock];
      while (!state.finished) {
        [state.condition wait];
      }
      [state.condition unlock];
      [session finishTasksAndInvalidate];

      const bool http_ok = state.status >= 200 && state.status < 300;
      result.ok = http_ok && state.error.empty() && !state.cancelled && !state.write_failed;
      if (state.cancelled) {
        result.error = "the transfer was cancelled";
        result.error_code = "cancelled";
      } else if (state.write_failed) {
        result.error = "cannot write the download to " + destination_path;
        result.error_code = "permission";
      } else if (state.error.empty() && state.status != 0 && !http_ok) {
        result.error = "the server answered with HTTP " + std::to_string(state.status);
        result.error_code = "network";
      }
      if (state.file >= 0) {
        close(state.file);
      }
      if (result.ok && destination_path.empty()) {
        result.body.assign((const char*)[state.body bytes], (size_t)[state.body length]);
      }
      if (!result.ok && !destination_path.empty()) {
        unlink(destination_path.c_str());
      }
    }
    return result;
  }
};

}  // namespace

HttpClient::HttpClient() : pimpl_(std::make_unique<MacosHttpImpl>()) {}

HttpClient::~HttpClient() = default;

}  // namespace dazzle
