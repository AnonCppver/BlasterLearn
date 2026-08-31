const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

const initializeNavigation = () => {
  const menuButton = document.querySelector('[data-menu-toggle]');
  const mainNav = document.querySelector('[data-main-nav]');
  if (!menuButton || !mainNav) return;

  menuButton.addEventListener('click', () => {
    const open = mainNav.classList.toggle('is-open');
    menuButton.setAttribute('aria-expanded', String(open));
  });

  mainNav.addEventListener('click', () => {
    mainNav.classList.remove('is-open');
    menuButton.setAttribute('aria-expanded', 'false');
  });
};

const initializeReadingProgress = () => {
  const progress = document.querySelector('[data-reading-progress]');
  if (!progress) return;

  const updateProgress = () => {
    const scrollable = document.documentElement.scrollHeight - window.innerHeight;
    const ratio = scrollable > 0 ? window.scrollY / scrollable : 0;
    progress.style.width = `${Math.min(1, Math.max(0, ratio)) * 100}%`;
  };

  updateProgress();
  window.addEventListener('scroll', updateProgress, { passive: true });
  window.addEventListener('resize', updateProgress);
};

const initializeReveal = () => {
  const items = document.querySelectorAll('[data-reveal]');
  if (reducedMotion || !('IntersectionObserver' in window)) {
    items.forEach((item) => item.classList.add('is-visible'));
    return;
  }

  const observer = new IntersectionObserver((entries, currentObserver) => {
    entries.forEach((entry) => {
      if (!entry.isIntersecting) return;
      entry.target.classList.add('is-visible');
      currentObserver.unobserve(entry.target);
    });
  }, { threshold: 0.12 });

  items.forEach((item) => observer.observe(item));
};

const initializeTrackedSections = () => {
  const sections = document.querySelectorAll('[data-track-section]');
  const links = document.querySelectorAll('[data-track-link]');
  if (!sections.length || !links.length || !('IntersectionObserver' in window)) return;

  const observer = new IntersectionObserver((entries) => {
    const visible = entries
      .filter((entry) => entry.isIntersecting)
      .sort((a, b) => b.intersectionRatio - a.intersectionRatio)[0];
    if (!visible) return;

    links.forEach((link) => {
      const current = link.getAttribute('href') === `#${visible.target.id}`;
      if (current) link.setAttribute('aria-current', 'true');
      else link.removeAttribute('aria-current');
    });
  }, { rootMargin: '-20% 0px -60%', threshold: [0.05, 0.25, 0.5] });

  sections.forEach((section) => observer.observe(section));
};

const initializePlaceholderLinks = () => {
  document.querySelectorAll('[data-placeholder-link]').forEach((link) => {
    link.addEventListener('click', (event) => event.preventDefault());
  });
};

const restoreInitialAnchor = () => {
  if (!window.location.hash) return;
  const target = document.querySelector(window.location.hash);
  if (target) requestAnimationFrame(() => target.scrollIntoView());
};

const startSite = () => {
  initializeNavigation();
  initializeReadingProgress();
  initializeReveal();
  initializeTrackedSections();
  initializePlaceholderLinks();
  restoreInitialAnchor();
};

startSite();
