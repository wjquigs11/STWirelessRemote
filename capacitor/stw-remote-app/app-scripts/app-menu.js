/* Capacitor-only hamburger menu. The ESP32 Wi-Fi menu is not bundled here. */
(function () {
  'use strict';

  var menuItems = [
    { href: '/', text: 'Home' },
    { href: '/settings.html', text: 'Settings' },
    { href: '#host-info', text: 'ESP32 host info', external: true }
  ];

  function controllerBase() {
    return window.StwDevice ? window.StwDevice.base() : 'http://ESPmcu.local';
  }

  function closeMenu(dropdown) {
    dropdown.classList.remove('show');
  }

  function createMenu() {
    if (document.querySelector('.hamburger-menu')) return;

    var menuContainer = document.createElement('div');
    menuContainer.className = 'hamburger-menu';

    var button = document.createElement('button');
    button.type = 'button';
    button.className = 'hamburger-icon';
    button.setAttribute('aria-label', 'Open menu');
    button.setAttribute('aria-expanded', 'false');

    var image = document.createElement('img');
    image.src = 'hamburger.png';
    image.alt = '';
    button.appendChild(image);

    var dropdown = document.createElement('div');
    dropdown.className = 'menu-dropdown';
    dropdown.id = 'menuDropdown';

    menuItems.forEach(function (item) {
      var link = document.createElement('a');
      link.href = item.external ? controllerBase() + '/host' : item.href;
      link.textContent = item.text;
      if (item.external) {
        link.target = '_system';
        link.rel = 'noopener';
      }
      link.addEventListener('click', function () {
        closeMenu(dropdown);
      });
      dropdown.appendChild(link);
    });

    button.addEventListener('click', function (event) {
      event.stopPropagation();
      var isOpen = dropdown.classList.toggle('show');
      button.setAttribute('aria-expanded', String(isOpen));
    });

    menuContainer.appendChild(button);
    menuContainer.appendChild(dropdown);
    document.body.appendChild(menuContainer);

    document.addEventListener('click', function (event) {
      if (!menuContainer.contains(event.target)) {
        closeMenu(dropdown);
        button.setAttribute('aria-expanded', 'false');
      }
    });
    document.addEventListener('keydown', function (event) {
      if (event.key === 'Escape') {
        closeMenu(dropdown);
        button.setAttribute('aria-expanded', 'false');
      }
    });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', createMenu);
  } else {
    createMenu();
  }
}());
