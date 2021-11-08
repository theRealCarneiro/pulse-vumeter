/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <math.h>
#include <pthread.h>


static pa_context *context = NULL;
static pa_stream *stream = NULL;
static char *device_name = NULL;
unsigned int last = 0;
unsigned int dev_type = 0;
float levels[2] = {-1.0, -1.0};
float dbRMS = 0;
int __main_loop = 0;

void show_error(const char *txt) {
    fprintf(stderr, "%s: %s\n", txt, pa_strerror(pa_context_errno(context)));
}

static void stream_read_callback(pa_stream *s, size_t length, void *dummy) {
    (void) dummy;
    const void *p;
    float *data = NULL;

	unsigned int samples = 0;


    if (pa_stream_peek(s, &p, &length) < 0) {
		fprintf(stderr, "pa_stream_peek() failed: %s", pa_strerror(pa_context_errno(context)));
        return;
    }

	samples = (length / sizeof(float));
    data = (float *) p;

	float v = 0;

	for(unsigned int i = 1; i < samples; i++){
		/*v += data[i] * data[i];*/
		v += fabs(data[i]); // * data[i];
	}
	v = v / samples;
	v = sqrt(v);

	/*v = ((const float*) data)[length / sizeof(float) -1];*/

	if (dbRMS <= v)
		dbRMS = (0.1 * dbRMS + 0.9 * v);
		/*dbRMS = v;*/
	/*else dbRMS -= .025;*/

	if (dbRMS > 1)
		dbRMS = 1;
	else if (dbRMS < 0)
		dbRMS = 0;

	pa_stream_drop(s);
}

static void stream_state_callback(pa_stream *s, void *dummy) {
    (void) dummy;

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY:
            /*printf("\n");*/
            break;

        case PA_STREAM_FAILED:
            show_error("Connection failed");
            break;
    }
}

static void create_stream(const char *name, const pa_sample_spec *ss, const pa_channel_map *cmap) {
    pa_sample_spec nss;

    nss.format = PA_SAMPLE_FLOAT32;
    nss.rate = ss->rate;
    nss.channels = ss->channels;

    stream = pa_stream_new(context, "Console Audio Meter", &nss, cmap);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
    pa_stream_connect_record(stream, name, NULL, (enum pa_stream_flags) 0);
}

static void context_get_sink_info_callback(pa_context *d, const pa_sink_info *si, int is_last, void *dummy) {
    (void) d;
    (void) dummy;

    if (is_last < 0) {
        show_error("Failed to get sink information");
        return;
    }

    if (!si)
        return;

    create_stream(si->monitor_source_name, &si->sample_spec, &si->channel_map);
}

static void context_get_source_info_callback(pa_context *d, const pa_source_info *si, int is_last, void *dummy) {
    (void) d;
    (void) dummy;

    if (is_last < 0) {
        show_error("Failed to get source information");
        return;
    }

    if (!si)
        return;

    create_stream(si->name, &si->sample_spec, &si->channel_map);
}

static void context_state_callback(pa_context *c, void *dummy) {
    (void) dummy;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        case PA_CONTEXT_TERMINATED:
            break;

        case PA_CONTEXT_READY:
            assert(!stream);
			if (dev_type == 1)
				pa_operation_unref(pa_context_get_source_info_by_name(c, device_name, 
							context_get_source_info_callback, NULL));
			else
				pa_operation_unref(pa_context_get_sink_info_by_name(c, device_name, 
							context_get_sink_info_callback, NULL));
            break;

        case PA_CONTEXT_FAILED:
            show_error("Connection failed");
            break;
    }
}

void *print_dbrms_thread( ){
	while (1){
		usleep(33000);
		fflush(stdout);
		printf("%.2f\n", dbRMS);
		if (dbRMS - 0.04 >= 0)
			dbRMS -= 0.04;
	}
}

int main(int argc, char* argv[]) {
    pa_mainloop* m = NULL;
	if (argc != 2){
		device_name = argv[1];
		dev_type = atoi(argv[2]);
	} else return 1;
	pthread_t thread1;
	pthread_create(&thread1, NULL, print_dbrms_thread, NULL);
    int ret = 0;

    signal(SIGPIPE, SIG_IGN);
    m = pa_mainloop_new();
    assert(m);

    context = pa_context_new(pa_mainloop_get_api(m), "Console Meter");
    assert(context);

    pa_context_set_state_callback(context, context_state_callback, NULL);
    pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

    if (pa_mainloop_run(m, &ret) < 0)
        fprintf(stderr, "pa_mainloop_run: %d\n", ret);

    return 0;
}
