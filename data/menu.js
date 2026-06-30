/**
 * Unified Hamburger Menu System
 * Dynamically creates and manages the hamburger menu for all pages
 */

class HamburgerMenu {
  constructor() {
    this.menuItems = [
      { href: '/', text: 'Home' },
      { href: '/setup.html', text: 'Setup' },
      { href: '/display.html', text: 'Display' },
      { href: '/wifimanager', text: 'WiFi' }
    ];
    
    this.init();
  }

  init() {
    this.injectCSS();
    this.createMenuHTML();
    this.attachEventListeners();
  }

  injectCSS() {
    const style = document.createElement('style');
    style.textContent = `
      /* Hamburger Menu Styles */
      .hamburger-menu {
        position: fixed;
        top: 20px;
        right: 20px;
        z-index: 1000;
      }
      
      .hamburger-icon {
        background: #333;
        border: none;
        padding: 8px;
        border-radius: 5px;
        cursor: pointer;
        width: 45px;
        height: 45px;
        display: flex;
        align-items: center;
        justify-content: center;
      }
      
      .hamburger-icon:hover {
        background: #555;
      }
      
      .hamburger-icon img {
        width: 24px;
        height: 24px;
        filter: invert(1);
      }
      
      .menu-dropdown {
        display: none;
        position: absolute;
        top: 50px;
        right: 0;
        background: white;
        border: 1px solid #ccc;
        border-radius: 5px;
        box-shadow: 0 2px 10px rgba(0,0,0,0.3);
        min-width: 200px;
        z-index: 1001;
        max-height: 400px;
        overflow-y: auto;
      }
      
      .menu-dropdown.show {
        display: block !important;
        visibility: visible !important;
        opacity: 1 !important;
      }
      
      .menu-dropdown a {
        display: block;
        padding: 12px 16px;
        text-decoration: none;
        color: #333;
        border-bottom: 1px solid #eee;
      }
      
      .menu-dropdown a:last-child {
        border-bottom: none;
      }
      
      .menu-dropdown a:hover {
        background-color: #f5f5f5;
      }
    `;
    document.head.appendChild(style);
  }

  createMenuHTML() {
    const menuContainer = document.createElement('div');
    menuContainer.className = 'hamburger-menu';
    
    const button = document.createElement('button');
    button.className = 'hamburger-icon';
    button.onclick = () => this.toggleMenu();
    
    const img = document.createElement('img');
    img.src = 'hamburger.png';
    img.alt = 'Menu';
    button.appendChild(img);
    
    const dropdown = document.createElement('div');
    dropdown.className = 'menu-dropdown';
    dropdown.id = 'menuDropdown';
    
    this.menuItems.forEach(item => {
      const link = document.createElement('a');
      link.href = item.href;
      link.textContent = item.text;
      dropdown.appendChild(link);
    });
    
    menuContainer.appendChild(button);
    menuContainer.appendChild(dropdown);
    document.body.appendChild(menuContainer);
  }

  toggleMenu() {
    const dropdown = document.getElementById('menuDropdown');
    if (dropdown) {
      dropdown.classList.toggle('show');
      if (dropdown.classList.contains('show')) {
        dropdown.style.display = 'block';
        dropdown.style.visibility = 'visible';
        dropdown.style.opacity = '1';
        dropdown.style.zIndex = '1001';
      }
    }
  }

  attachEventListeners() {
    document.addEventListener('click', (event) => {
      const menu = document.querySelector('.hamburger-menu');
      const dropdown = document.getElementById('menuDropdown');
      
      if (menu && dropdown && !menu.contains(event.target)) {
        dropdown.classList.remove('show');
      }
    });

    document.addEventListener('keydown', (event) => {
      if (event.key === 'Escape') {
        const dropdown = document.getElementById('menuDropdown');
        if (dropdown) {
          dropdown.classList.remove('show');
        }
      }
    });
  }
}

function initializeMenu() {
  try {
    new HamburgerMenu();
  } catch (error) {
    console.error('Error initializing hamburger menu:', error);
  }
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initializeMenu);
} else {
  initializeMenu();
}
