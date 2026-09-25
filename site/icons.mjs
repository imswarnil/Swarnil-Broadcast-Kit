/*  The nav icons, as inline SVG.

    Inline rather than a sprite or a font: there are six of them, they are
    twenty lines each, and a sprite would be one more request and one more file
    to keep in step for no gain at this size. They inherit `currentColor`, so
    they follow the theme without a second set.

    All drawn on the same 24 grid with the same 1.75 stroke, because icons from
    different grids sitting in a row is the thing that makes a nav look
    assembled rather than designed.  */

const wrap = (d) =>
	`<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${d}</svg>`;

export const icons = {
	overview: wrap('<circle cx="12" cy="12" r="3.2"/><path d="M12 4.2v1.6M12 18.2v1.6M4.2 12h1.6M18.2 12h1.6"/><circle cx="12" cy="12" r="8.4" opacity=".45"/>'),
	scenes: wrap('<rect x="2.6" y="5" width="13.6" height="10" rx="1.8"/><path d="M18.8 7.4h1.2a1.4 1.4 0 0 1 1.4 1.4v7.8a1.4 1.4 0 0 1-1.4 1.4H8.6a1.4 1.4 0 0 1-1.4-1.4v-.6" opacity=".55"/>'),
	sources: wrap('<rect x="3" y="3" width="7.2" height="7.2" rx="1.6"/><rect x="13.8" y="3" width="7.2" height="7.2" rx="1.6" opacity=".55"/><rect x="3" y="13.8" width="7.2" height="7.2" rx="1.6" opacity=".55"/><rect x="13.8" y="13.8" width="7.2" height="7.2" rx="1.6"/>'),
	builder: wrap('<path d="M3.4 3.4h7v5.2h-7zM3.4 12.4h7v8.2h-7zM13.6 3.4h7v8.2h-7z" opacity=".55"/><path d="M13.6 15.4h7v5.2h-7z"/><path d="M12 12h.01"/>'),
	docs: wrap('<path d="M5 3.6h9.2L19 8.4v12H5z"/><path d="M14 3.6v5h5" opacity=".55"/><path d="M8.4 12.6h7.2M8.4 16.2h4.8" opacity=".8"/>'),
	remote: wrap('<rect x="7.6" y="2.6" width="8.8" height="18.8" rx="2.4"/><path d="M11 5.6h2" opacity=".55"/><circle cx="12" cy="17" r="1.1"/><path d="M9.8 9.4h4.4M9.8 12.2h4.4" opacity=".7"/>'),
};

/* the recording light, which is the kit's whole idea in one shape */
export const mark = `<svg viewBox="0 0 24 24" aria-hidden="true" class="mark"><circle cx="12" cy="12" r="11" class="mark__ring"/><circle cx="12" cy="12" r="5.4" class="mark__dot"/></svg>`;
