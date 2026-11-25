(function () {
  'use strict';

  function init() {
    var form = document.getElementById('controller-form');
    var input = document.getElementById('controller-host');
    var status = document.getElementById('controller-status');
    if (!form || !input || !window.StwDevice) return;

    input.value = window.StwDevice.getController();

    var test = document.getElementById('controller-test');
    if (test) {
      test.addEventListener('click', function () {
        var value = input.value.trim();
        if (!window.StwDevice.setController(value)) {
          status.textContent = 'Enter a hostname or IP address without a path.';
          return;
        }
        status.textContent = 'Connecting...';
        var controller = new AbortController();
        var timer = setTimeout(function () { controller.abort(); }, 5000);
        fetch(window.StwDevice.base() + '/host', { signal: controller.signal })
          .then(function (r) { return r.text(); })
          .then(function (t) { clearTimeout(timer); status.textContent = 'OK: ' + t; })
          .catch(function () { clearTimeout(timer); status.textContent = 'Unable to reach the ESP32.'; });
      });
    }
    form.addEventListener('submit', function (event) {
      event.preventDefault();
      var value = input.value.trim();
      if (!window.StwDevice.setController(value)) {
        status.textContent = 'Enter a hostname or IP address without a path.';
        return;
      }
      window.location.href = '/';
    });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
}());
