// gameswf_sound_handler_sdl.cpp	-- Thatcher Ulrich http://tulrich.com 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// A gameswf::sound_handler that uses SDL_mixer for output


#include "gameswf.h"
#include "base/container.h"
#include "SDL_mixer.h"


// Use SDL_mixer to handle gameswf sounds.
struct SDL_sound_handler : gameswf::sound_handler
{
	bool	m_opened;
	array<Mix_Chunk*>	m_samples;

	#define	SAMPLE_RATE 22050
	#define MIX_CHANNELS 8
	#define STEREO_MONO_CHANNELS 1

	SDL_sound_handler()
		:
		m_opened(false)
	{
		if (Mix_OpenAudio(SAMPLE_RATE, AUDIO_S16SYS, STEREO_MONO_CHANNELS, 1024) != 0)
		{
			printf("can't open SDL_mixer!");
			printf(Mix_GetError());
		}
		else
		{
			m_opened = true;

			Mix_AllocateChannels(MIX_CHANNELS);
		}
	}

	~SDL_sound_handler()
	{
		if (m_opened)
		{
			Mix_CloseAudio();
			for (int i = 0, n = m_samples.size(); i < n; i++)
			{
				if (m_samples[i])
				{
					Mix_FreeChunk(m_samples[i]);
				}
			}
		}
		else
		{
			assert(m_samples.size() == 0);
		}
	}


	virtual int	create_sound(
		void* data,
		int data_bytes,
		int sample_count,
		format_type format,
		int sample_rate,
		bool stereo)
	// Called by gameswf to create a sample.  We'll return a sample ID that gameswf
	// can use for playing it.
	{
		if (m_opened == false)
		{
			return 0;
		}

		Sint16*	adjusted_data = 0;
		int	adjusted_size = 0;
		Mix_Chunk*	sample = 0;

		switch (format)
		{
		case FORMAT_RAW:
			convert_raw_data(&adjusted_data, &adjusted_size, data, sample_count, 1, sample_rate, stereo);
			break;

		case FORMAT_NATIVE16:
			convert_raw_data(&adjusted_data, &adjusted_size, data, sample_count, 2, sample_rate, stereo);
			break;

		case FORMAT_MP3:
			printf("mp3 format sound requested; this demo does not handle mp3\n");
			break;

		default:
			// Unhandled format.
			printf("unknown format sound requested; this demo does not handle it\n");
			break;
		}

		if (adjusted_data)
		{
			sample = Mix_QuickLoad_RAW((unsigned char*) adjusted_data, adjusted_size);
			Mix_VolumeChunk(sample, 128);	// full volume by default
		}

		m_samples.push_back(sample);
		return m_samples.size() - 1;
	}


	virtual void	play_sound(int sound_handle, int loop_count /* other params */)
	// Play the index'd sample.
	{
		if (sound_handle >= 0 && sound_handle < m_samples.size())
		{
			if (m_samples[sound_handle])
			{
				// Play this sample on the first available channel.
				Mix_PlayChannel(-1, m_samples[sound_handle], loop_count);
			}
		}
	}

	
	virtual void	stop_sound(int sound_handle)
	{
		if (sound_handle < 0 || sound_handle >= m_samples.size())
		{
			// Invalid handle.
			return;
		}

		for (int i = 0; i < MIX_CHANNELS; i++)
		{
			Mix_Chunk*	playing_chunk = Mix_GetChunk(i);
			if (Mix_Playing(i)
			    && playing_chunk == m_samples[sound_handle])
			{
				// Stop this channel.
				Mix_HaltChannel(i);
			}
		}
	}


	virtual void	delete_sound(int sound_handle)
	// gameswf calls this when it's done with a sample.
	{
		if (sound_handle >= 0 && sound_handle < m_samples.size())
		{
			Mix_Chunk*	chunk = m_samples[sound_handle];
			if (chunk)
			{
				delete [] (chunk->abuf);
				Mix_FreeChunk(chunk);
				m_samples[sound_handle] = 0;
			}
		}
	}


	static void convert_raw_data(
		Sint16** adjusted_data,
		int* adjusted_size,
		void* data,
		int sample_count,
		int sample_size,
		int sample_rate,
		bool stereo)
	// VERY crude sample-rate & sample-size conversion.  Converts
	// input data to the SDL_mixer output format (SAMPLE_RATE,
	// stereo, 16-bit native endianness)
	{
// 		// xxxxx debug pass-thru
// 		{
// 			int	output_sample_count = sample_count * (stereo ? 2 : 1);
// 			Sint16*	out_data = new Sint16[output_sample_count];
// 			*adjusted_data = out_data;
// 			*adjusted_size = output_sample_count * 2;	// 2 bytes per sample
// 			memcpy(out_data, data, *adjusted_size);
// 			return;
// 		}
// 		// xxxxx

//		if (stereo == false) { sample_rate >>= 1; }	// simple hack to handle dup'ing mono to stereo
		if (stereo == true) { sample_rate <<= 1; }	// simple hack to lose half the samples to get mono from stereo

		// Brain-dead sample-rate conversion: duplicate or
		// skip input samples an integral number of times.
		int	inc = 1;	// increment
		int	dup = 1;	// duplicate
		if (sample_rate > SAMPLE_RATE)
		{
			inc = sample_rate / SAMPLE_RATE;
		}
		else if (sample_rate < SAMPLE_RATE)
		{
			dup = SAMPLE_RATE / sample_rate;
		}

		int	output_sample_count = (sample_count * dup) / inc;
		Sint16*	out_data = new Sint16[output_sample_count];
		*adjusted_data = out_data;
		*adjusted_size = output_sample_count * 2;	// 2 bytes per sample

		if (sample_size == 1)
		{
			// Expand from 8 bit to 16 bit.
			Uint8*	in = (Uint8*) data;
			for (int i = 0; i < output_sample_count; i++)
			{
				Uint8	val = *in;
				for (int j = 0; j < dup; j++)
				{
					*out_data++ = (int(val) - 128);
				}
				in += inc;
			}
		}
		else
		{
			// 16-bit to 16-bit conversion.
			Sint16*	in = (Sint16*) data;
			for (int i = 0; i < output_sample_count; i += dup)
			{
				Sint16	val = *in;
				for (int j = 0; j < dup; j++)
				{
					*out_data++ = val;
				}
				in += inc;
			}
		}
	}

};


gameswf::sound_handler*	create_sound_handler_sdl()
// Factory.
{
	return new SDL_sound_handler;
}


void	delete_sound_handler_sdl(gameswf::sound_handler* h)
// Destructor.
{
	delete (SDL_sound_handler*) h;
}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
