/*  Two small things, and nothing else on the site.

    The theme button, and a highlight for whatever a deep link pointed at. */

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
