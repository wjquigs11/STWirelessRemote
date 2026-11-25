/*
 * Rewrite firmware-relative requests for the Capacitor WebView.
 * The ESP32 web UI uses /events, /SendCommand.html, /trollmode, and /n2k.json paths. Inside the
 * native app those paths would otherwise resolve to the bundled app origin.
 */
(function () {
  'use strict';

  var KEY_CONTROLLER = 'stwremote.controller';
  var DEFAULT_CONTROLLER = 'ESPmcu.local';

  function normalizeHost(value) {
    return String(value || '').trim()
      .replace(/^https?:\/\//i, '')
      .replace(/\/+$/, '');
  }

  function getController() {
    try {
      return normalizeHost(localStorage.getItem(KEY_CONTROLLER)) || DEFAULT_CONTROLLER;
    } catch (e) {
      return DEFAULT_CONTROLLER;
    }
  }

  function setController(value) {
    var host = normalizeHost(value);
    if (!host || /[\/?#\s]/.test(host)) return false;
    try { localStorage.setItem(KEY_CONTROLLER, host); } catch (e) {}
    return true;
  }

  function isIp(host) {
    return /^\d{1,3}(\.\d{1,3}){3}$/.test(host);
  }

  function base() {
    var host = getController();
    // A public HTTPS proxy can be entered manually; local ESP32 hosts use HTTP.
    var https = !isIp(host) && !/\.local$/i.test(host) && host.indexOf('.') !== -1;
    return (https ? 'https://' : 'http://') + host;
  }

  function rewriteUrl(value) {
    if (value == null) return value;
    var url = String(value);
    if (url.charAt(0) === '/') return base() + url;

    if (/^(https?:|capacitor:)/i.test(url)) {
      try {
        var parsed = new URL(url, window.location.href);
        if (parsed.hostname === window.location.hostname || parsed.hostname === 'localhost') {
          return base() + parsed.pathname + parsed.search + parsed.hash;
        }
      } catch (e) {}
    }
    return url;
  }

  var nativeFetch = window.fetch ? window.fetch.bind(window) : null;
  if (nativeFetch) {
    window.fetch = function (input, init) {
      var rewritten = input;
      try {
        if (typeof input === 'string') {
          rewritten = rewriteUrl(input);
        } else if (input && input.url) {
          var requestUrl = rewriteUrl(input.url);
          if (requestUrl !== input.url) rewritten = new Request(requestUrl, input);
        }
      } catch (e) {
        console.warn('stwremote: request rewrite failed', e);
      }
      return nativeFetch(rewritten, init);
    };
  }

  // Rewrite the legacy XHR calls used by the shared firmware pages.
  var nativeOpen = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function (method, url) {
    var args = Array.prototype.slice.call(arguments);
    args[1] = rewriteUrl(url);
    return nativeOpen.apply(this, args);
  };
  function rewriteFormActions() {
    document.querySelectorAll('form[action]').forEach(function (form) {
      form.setAttribute('action', rewriteUrl(form.getAttribute('action')));
    });
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', rewriteFormActions);
  } else {
    rewriteFormActions();
  }
  // Keep SSE enabled. The remote page receives its live readings through
  // the firmware's /events EventSource.
  var NativeEventSource = window.EventSource;
  if (NativeEventSource) {
    window.EventSource = function (url, config) {
      return new NativeEventSource(rewriteUrl(url), config);
    };
    window.EventSource.prototype = NativeEventSource.prototype;
    window.EventSource.CONNECTING = NativeEventSource.CONNECTING;
    window.EventSource.OPEN = NativeEventSource.OPEN;
    window.EventSource.CLOSED = NativeEventSource.CLOSED;
  }

  window.StwDevice = {
    getController: getController,
    setController: setController,
    base: base,
    defaultController: DEFAULT_CONTROLLER
  };
}());
