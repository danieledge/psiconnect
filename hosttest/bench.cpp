// bench.cpp - how long the Atom's own web proxy code takes on this PC, and
// how much memory it needs, for saved pages and pictures: the HTML
// simplifier, and pictures to 16-grey GIFs (the 5mx) and to GIFs in EPOC's
// 256 colours (the netBook, 2.8). The figures are this PC's; the README's
// "The netBook" section turns them into estimates for the AtomS3R.
//
//   make -C hosttest bench
//   hosttest/bench FILE...       (pages end in .html; anything else is a picture)
//   BENCH_SAVE=DIR hosttest/bench ...   (also keeps the GIFs it made in DIR)
//
// MIT licence (see LICENSE at the top of the repository).
#include "htmlsimp.h"
#include "imgconv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <chrono>

struct CountSink : am::HtmlSink
	{
	size_t n = 0;
	std::string s;
	bool keep = false;
	void Put(const char* d, size_t len) override { n += len; if (keep) s.append(d, len); }
	};

static std::string File(const char* aPath)
	{
	FILE* f = fopen(aPath, "rb");
	if (!f) return "";
	std::string s;
	char b[65536];
	size_t n;
	while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
	fclose(f);
	return s;
	}

static double Ms(std::chrono::steady_clock::time_point a)
	{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
	}

static const char* Base(const char* p)
	{
	const char* b = strrchr(p, '/');
	return b ? b + 1 : p;
	}

int main(int argc, char** argv)
	{
	const int kRuns = 5;
	printf("%-22s %-7s %9s %9s %9s %9s %10s\n", "file", "what", "in", "out", "out w x h", "ms (PC)", "memory KB");
	for (int i = 1; i < argc; i++)
		{
		std::string d = File(argv[i]);
		if (d.empty())
			continue;
		const char* name = Base(argv[i]);
		if (strstr(name, ".html"))
			{
			for (int mode = 0; mode < 2; mode++)
				{
				int m = mode ? am::HtmlSimplifier::EReader : am::HtmlSimplifier::EKeepPictures;
				double best = 1e9;
				size_t out = 0;
				for (int r = 0; r < kRuns; r++)
					{
					CountSink s;
					am::HtmlSimplifier h;
					auto t = std::chrono::steady_clock::now();
					h.Begin(&s, m, 0);
					for (size_t k = 0; k < d.size(); k += 1460)   // as it comes from TCP
						h.Feed((const uint8_t*)d.data() + k, d.size() - k < 1460 ? d.size() - k : 1460);
					h.End();
					double ms = Ms(t);
					if (ms < best) best = ms;
					out = s.n;
					}
				printf("%-22.22s %-7s %9zu %9zu %9s %9.2f %10s\n", name, mode ? "reader" : "html", d.size(), out, "-",
					best, "streams");
				}
			continue;
			}
		struct { const char* what; int width; bool colour; } runs[] = {
			{ "grey300", 300, false }, { "nb-col", 560, true } };
		for (auto& run : runs)
			{
			double best = 1e9;
			size_t out = 0;
			int w = 0, h = 0, sw = 0, sh = 0;
			bool ok = false, colour = false;
			char why[64] = "";
			for (int r = 0; r < kRuns; r++)
				{
				am::ImageConverter c;
				CountSink s;
				auto t = std::chrono::steady_clock::now();
				ok = c.Decode((const uint8_t*)d.data(), d.size(), run.width, why, sizeof(why), run.colour);
				if (ok)
					while (!c.Write(&s, 64))
						;
				double ms = Ms(t);
				if (ms < best) best = ms;
				out = s.n;
				w = c.Width(); h = c.Height(); sw = c.SourceWidth(); sh = c.SourceHeight();
				colour = c.Colour();
				const char* save = getenv("BENCH_SAVE");   // a folder: the GIFs are kept there
				if (save && ok && r == 0)
					{
					char path[512];
					snprintf(path, sizeof(path), "%s/%s-%s.gif", save, name, run.what);
					s.keep = true;
					am::ImageConverter c2;
					if (c2.Decode((const uint8_t*)d.data(), d.size(), run.width, why, sizeof(why), run.colour))
						while (!c2.Write(&s, 64))
							;
					FILE* f = fopen(path, "wb");
					if (f) { fwrite(s.s.data(), 1, s.s.size(), f); fclose(f); }
					}
				}
			if (!ok)
				{
				printf("%-22.22s %-7s %9zu %9s %9s %9.2f %10s  (%s)\n", name, run.what, d.size(), "-", "-", best, "-", why);
				continue;
				}
			// memory while converting: the source file, the result (a byte a
			// pixel in colour, half that in greys), the GIF gathered whole
			// (at most 1.5 bytes a pixel), the LZW dictionary (30 KB colour,
			// 12 KB greys) and the decoder's rows (a row of MCUs: 16 rows of
			// the source at 1 or 3 bytes a pixel)
			double px = (double)w * h;
			double mem = d.size() + px * (colour ? 1.0 : 0.5) + out + (colour ? 30.0 : 12.0) * 1024 +
				(double)sw * 16 * (colour ? 3 : 1);
			char wh[32];
			snprintf(wh, sizeof(wh), "%dx%d", w, h);
			printf("%-22.22s %-7s %9zu %9zu %9s %9.2f %10.0f%s\n", name, run.what, d.size(), out, wh, best,
				mem / 1024, run.colour && !colour ? "  (greys: not in colour)" : "");
			}
		}
	return 0;
	}
