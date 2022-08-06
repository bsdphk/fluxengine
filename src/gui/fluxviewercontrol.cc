#include "globals.h"
#include "gui.h"
#include "fluxviewercontrol.h"
#include "lib/flux.h"
#include "lib/fluxmap.h"
#include "lib/sector.h"
#include "lib/decoders/fluxmapreader.h"

DECLARE_COLOUR(BACKGROUND, 192, 192, 192);
DECLARE_COLOUR(READ_SEPARATOR, 255, 0, 0);
DECLARE_COLOUR(INDEX_SEPARATOR, 255, 255, 0);
DECLARE_COLOUR(FOREGROUND, 0, 0, 0);
DECLARE_COLOUR(FLUX, 64, 64, 255);
DECLARE_COLOUR(RECORD, 255, 255, 255);

const int BORDER = 4;
const int MINIMUM_TICK_DISTANCE = 10;
const double RENDER_LIMIT = 3000.0;

FluxViewerControl::FluxViewerControl(wxWindow* parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style):
    wxWindow(parent, id, pos, size, style, "FluxViewerControl")
{
	SetDoubleBuffered(true);
}

wxBEGIN_EVENT_TABLE(FluxViewerControl, wxPanel)
    EVT_PAINT(FluxViewerControl::OnPaint)
	EVT_MOUSEWHEEL(FluxViewerControl::OnMouseWheel)
wxEND_EVENT_TABLE()

void FluxViewerControl::SetScrollbar(wxScrollBar* scrollbar)
{
	_scrollbar = scrollbar;
	_scrollbar->Bind(wxEVT_SCROLL_THUMBTRACK, &FluxViewerControl::OnScrollbarChanged, this);
	_scrollbar->Bind(wxEVT_SCROLL_CHANGED, &FluxViewerControl::OnScrollbarChanged, this);
}
	
void FluxViewerControl::SetFlux(std::shared_ptr<const TrackFlux> flux)
{
	_flux = flux;

	_scrollPosition = 0;
	_totalDuration = 0;
	for (const auto& trackdata : _flux->trackDatas)
		_totalDuration += trackdata->fluxmap->duration();

	auto size = GetSize();
	_nanosecondsPerPixel = (double)_totalDuration / (double)size.GetWidth();

	UpdateScale();
	Refresh();
}

void FluxViewerControl::UpdateScale()
{
	auto size = GetSize();
	nanoseconds_t thumbSize = size.GetWidth() * _nanosecondsPerPixel;
	_scrollbar->SetScrollbar(_scrollPosition/1000, thumbSize/1000, _totalDuration/1000, thumbSize/2000);

	int totalPixels = (_totalDuration / _nanosecondsPerPixel) + 1;
	if ((totalPixels != _densityMap.size()) && (_nanosecondsPerPixel > RENDER_LIMIT))
	{
		_densityMap.resize(totalPixels);
		std::fill(_densityMap.begin(), _densityMap.end(), 0.0);

		int i = 0;
		for (const auto& trackdata : _flux->trackDatas)
		{
			FluxmapReader fmr(*trackdata->fluxmap);
			while (!fmr.eof())
			{
				unsigned ticks;
				if (!fmr.findEvent(F_BIT_PULSE, ticks))
					break;

				int fx = fmr.tell().ns() / _nanosecondsPerPixel;
				_densityMap.at(i + fx)++;
			}
			i += trackdata->fluxmap->duration() / _nanosecondsPerPixel;
		}

		double max = *std::max_element(_densityMap.begin(), _densityMap.end());
		for (auto& d : _densityMap)
			d /= max;
	}
}

static int interpolate(int lo, int hi, float factor)
{
	float range = hi - lo;
	return lo + range*factor;
}

void FluxViewerControl::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(this);
	dc.SetBackground(BACKGROUND_BRUSH);
	dc.Clear();

	auto size = GetSize();
	int w = size.GetWidth();
	int h = size.GetHeight();
	int th = h / 4;
	int ch = th * 3 / 4;
	int ch2 = ch / 2;
	int t1y = th/2;
	int t2y = th + th/2;
	int t3y = th*2 + th/2;
	int t4y = th*3 + th/2;

	int x = -_scrollPosition / _nanosecondsPerPixel;
	for (const auto& trackdata : _flux->trackDatas)
	{
		nanoseconds_t duration = trackdata->fluxmap->duration();
		int fw = duration / _nanosecondsPerPixel;

		if (((x+fw) > 0) && (x < w))
		{
			dc.SetPen(READ_SEPARATOR_PEN);
			dc.DrawLine({x, 0}, {x, h});

			dc.SetPen(FOREGROUND_PEN);
			dc.DrawLine({x, t1y}, {x+fw, t1y});
			dc.DrawLine({x, t2y}, {x+fw, t2y});
			dc.DrawLine({x, t3y}, {x+fw, t3y});

			/* Draw the horizontal scale. */

			nanoseconds_t tickStep = 1000;
			while ((tickStep / _nanosecondsPerPixel) < MINIMUM_TICK_DISTANCE)
				tickStep *= 10;

			static wxFont font(6, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
			dc.SetFont(font);
			dc.SetBackgroundMode(wxTRANSPARENT);
			dc.SetTextForeground(*wxBLACK);

			nanoseconds_t tick = 0;
			int tickCount = 0;
			while (tick < duration)
			{
				int xx = tick / _nanosecondsPerPixel;
				int ts = ch2/3;
				if ((tickCount % 10) == 0)
					ts = ch2/2;
				if ((tickCount % 100) == 0)
					ts = ch2;
				dc.DrawLine({x+xx, t3y - ts}, {x+xx, t3y + ts});
				if ((tickCount % 10) == 0)
				{
					dc.DrawText(
						fmt::format("{}us", tick / 1000LL),
						{ x+xx, t3y - ch2 }
					);
				}

				tick += tickStep;
				tickCount++;
			}

			if (x <= 0)
				dc.DrawText(
					fmt::format("{}us", (int)(_scrollPosition / 1000LL)),
					{ BORDER, t3y + ch2/2 }
				);

			if ((x+fw) >= w)
			{
				wxString text = fmt::format(
					"{}us", (int)((_scrollPosition + (w * _nanosecondsPerPixel)) / 1000LL));
				auto size = dc.GetTextExtent(text);
				dc.DrawText(text, { w - size.GetWidth() - BORDER, t3y + ch2/2 });
			}

			/* Sector blocks. */

			for (const auto& sector : trackdata->sectors)
			{
				int sp = sector->headerStartTime / _nanosecondsPerPixel;
				int sw = (sector->dataEndTime - sector->headerStartTime) / _nanosecondsPerPixel;

				dc.SetPen(FOREGROUND_PEN);
				dc.SetBrush(RECORD_BRUSH);
				dc.DrawRectangle({x+sp, t1y - ch2}, {sw, ch});
			}

			/* Record blocks. */

			for (const auto& record : trackdata->records)
			{
				int rp = record->startTime / _nanosecondsPerPixel;
				int rw = (record->endTime - record->startTime) / _nanosecondsPerPixel;

				dc.SetPen(FOREGROUND_PEN);
				dc.SetBrush(RECORD_BRUSH);
				dc.DrawRectangle({x+rp, t2y - ch2}, {rw, ch});
			}

			/* Flux chart. */

			dc.SetPen(FLUX_PEN);
			if (_nanosecondsPerPixel > RENDER_LIMIT)
			{
				/* Draw using density map. */

				dc.SetPen(*wxTRANSPARENT_PEN);
				for (int fx = 0; fx < _densityMap.size(); fx++)
				{
					if (((x+fx) > 0) && ((x+fx) < w))
					{
						float density = _densityMap[fx];
						wxColour colour(
							interpolate(BACKGROUND_COLOUR.Red(), FLUX_COLOUR.Red(), density),
							interpolate(BACKGROUND_COLOUR.Green(), FLUX_COLOUR.Green(), density),
							interpolate(BACKGROUND_COLOUR.Blue(), FLUX_COLOUR.Blue(), density));
						wxBrush brush(colour);
						dc.SetBrush(brush);
						dc.DrawRectangle({x+fx, t4y - ch2}, {1, ch});
					}
				}
			}
			else
			{
				/* Draw discrete pulses. */

				FluxmapReader fmr(*trackdata->fluxmap);
				while (!fmr.eof())
				{
					int event;
					unsigned ticks;
					fmr.getNextEvent(event, ticks);

					int fx = fmr.tell().ns() / _nanosecondsPerPixel;
					if (((x+fx) > 0) && ((x+fx) < w))
					{
						if (event & F_BIT_INDEX)
						{
							dc.SetPen(INDEX_SEPARATOR_PEN);
							dc.DrawLine({x+fx, 0}, {x+fx, h});
							dc.SetPen(FLUX_PEN);
						}

						if (event & F_BIT_PULSE)
							dc.DrawLine({x+fx, t4y - ch2}, {x+fx, t4y + ch2});
					}
				}
			}

			dc.SetPen(FLUX_PEN);
			dc.DrawLine({x, t4y}, {x+fw, t4y});
		}

		x += w;
	}
}

void FluxViewerControl::OnMouseWheel(wxMouseEvent& event)
{
    wxClientDC dc(this);
	int x = event.GetLogicalPosition(dc).x;
	
	_scrollPosition += x * _nanosecondsPerPixel;
	if (event.GetWheelRotation() > 0)
		_nanosecondsPerPixel /= 1.2;
	else
		_nanosecondsPerPixel *= 1.2;
	_scrollPosition -= x * _nanosecondsPerPixel;

	UpdateScale();
	Refresh();
}

void FluxViewerControl::OnScrollbarChanged(wxScrollEvent& event)
{
	_scrollPosition = event.GetPosition() * 1000LL;
	Refresh();
}

