# deps/include

The **libobs** and **obs-frontend-api** headers matching OBS Studio 32.2.2, and
the generated `obsconfig.h`. OBS.app ships the compiled frameworks but not the
headers, so a macOS build has nothing to compile against without these.

They are part of OBS Studio, © the OBS Project, licensed **GPL-2.0-or-later**
(`LICENSE` in this folder). Tally's own code is MIT; a compiled `tally.plugin`
links libobs and is distributed under the GPL's terms as a result.

Refresh them from an OBS source checkout of the same version:

```bash
rsync -a --include='*/' --include='*.h' --include='*.hpp' --exclude='*' obs-studio/libobs/ deps/include/libobs/
cp obs-studio/frontend/api/obs-frontend-api.h deps/include/obs-frontend-api/
```
