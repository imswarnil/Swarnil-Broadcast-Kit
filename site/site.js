/*  Three small things, and nothing else on the site.

    The theme button, the menu on a narrow screen, and a highlight for whatever
    a deep link pointed at. */

/* ---- theme ---------------------------------------------------------------- */

/* The head script has already applied a stored choice. This only has to handle
   the press — and the first press has to land on the opposite of what is on
   screen, which is the system theme when nothing is stored yet. */
const root = document.documentElement;
const current = () =>
	root.dataset.theme || (matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark');

for (const btn of document.querySelectorAll('[data-theme-toggle]')) {
	btn.addEventListener('click', () => {
		const next = current() === 'dark' ? 'light' : 'dark';
		root.dataset.theme = next;
		try {
			localStorage.setItem('sbk-theme', next);
		} catch (e) {
			/* private window, blocked storage: the theme still switches for
			   this page, it just will not be remembered */
		}
	});
}

/* Someone who has never pressed the button keeps following their system, even
   if they change it while the page is open. */
matchMedia('(prefers-color-scheme: light)').addEventListener('change', () => {
	let stored = null;
	try {
		stored = localStorage.getItem('sbk-theme');
	} catch (e) {}
	if (!stored) delete root.dataset.theme;
});

/* ---- the lightbox ---------------------------------------------------------- */

/* The shots are buttons rather than links, so this is the only way to see one
   full size — which means it has to work with the keyboard and close the way a
   dialog is expected to. <dialog> gives Escape and the backdrop for free. */
const box = document.getElementById('lightbox');
if (box) {
	const img = box.querySelector('img');
	const title = box.querySelector('strong');
	for (const btn of document.querySelectorAll('[data-shot]')) {
		btn.addEventListener('click', () => {
			img.src = btn.dataset.shot;
			img.alt = 'The ' + btn.dataset.title + ' scene, full size';
			title.textContent = btn.dataset.title;
			box.showModal();
		});
	}
	box.querySelector('[data-close]').addEventListener('click', () => box.close());
	/* clicking the darkness closes it; clicking the picture does not */
	box.addEventListener('click', (e) => {
		if (e.target === box) box.close();
	});
	/* drop the source on close so a big JPEG is not held for the whole session */
	box.addEventListener('close', () => {
		img.removeAttribute('src');
	});
}

/* ---- deep links ----------------------------------------------------------- */

const flash = () => {
	const id = location.hash.slice(1);
	if (!id) return;
	const el = document.getElementById(id);
	if (!el) return;
	el.style.transition = 'background-color 1.2s ease';
	el.style.backgroundColor = 'rgb(245 39 63 / 9%)';
	setTimeout(() => (el.style.backgroundColor = 'transparent'), 900);
};
addEventListener('hashchange', flash);
flash();

/* ---- the menu -------------------------------------------------------------- */

/*  Open and shut, and every way out of it that a person expects: the button
    again, Escape, a click anywhere else, following a link, and the viewport
    growing wide enough that the panel turns back into a row. Miss one and the
    menu is the thing on the site that feels broken.  */
const burger = document.querySelector('[data-menu-toggle]');
const sitenav = document.getElementById('sitenav');

if (burger && sitenav) {
	const setOpen = (open) => {
		burger.setAttribute('aria-expanded', String(open));
		if (open) sitenav.setAttribute('data-open', '');
		else sitenav.removeAttribute('data-open');
	};

	burger.addEventListener('click', (e) => {
		e.stopPropagation();
		setOpen(burger.getAttribute('aria-expanded') !== 'true');
	});

	sitenav.addEventListener('click', (e) => {
		if (e.target.closest('a')) setOpen(false);
	});

	document.addEventListener('click', (e) => {
		if (burger.getAttribute('aria-expanded') !== 'true') return;
		if (!sitenav.contains(e.target) && !burger.contains(e.target)) setOpen(false);
	});

	document.addEventListener('keydown', (e) => {
		if (e.key !== 'Escape' || burger.getAttribute('aria-expanded') !== 'true') return;
		setOpen(false);
		burger.focus();
	});

	/* the panel is a media-query away from being a row again; leaving it marked
	   open would hand the wide layout a nav with data-open on it */
	const wide = matchMedia('(min-width: 52.001rem)');
	wide.addEventListener('change', (e) => {
		if (e.matches) setOpen(false);
	});
}

