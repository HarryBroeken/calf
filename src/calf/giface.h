/* Calf DSP Library
 * API wrappers for Linux audio standards (apart from JACK, which is
 * a separate file now!)
 *
 * Copyright (C) 2007 Krzysztof Foltman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CALF_GIFACE_H
#define __CALF_GIFACE_H

#include <stdint.h>
#include <stdlib.h>
#if USE_LADSPA
#include <ladspa.h>
#endif
#if USE_JACK
#include <jack/jack.h>
#include <jack/midiport.h>
#endif
#include <exception>
#include <string>
#include "primitives.h"

namespace synth {

enum parameter_flags
{
  PF_TYPEMASK = 0x000F,
  PF_FLOAT = 0x0000,
  PF_INT = 0x0001,
  PF_BOOL = 0x0002,
  PF_ENUM = 0x0003,
  PF_ENUM_MULTI = 0x0004, 
  
  PF_SCALEMASK = 0x0FF0,
  PF_SCALE_DEFAULT = 0x0000,
  PF_SCALE_LINEAR = 0x0010,
  PF_SCALE_LOG = 0x0020,
  PF_SCALE_GAIN = 0x0030,
  PF_SCALE_PERC = 0x0040,

  PF_CTLMASK = 0x0F000,
  PF_CTL_DEFAULT = 0x00000,
  PF_CTL_KNOB = 0x01000,
  PF_CTL_FADER = 0x02000,
  PF_CTL_TOGGLE = 0x03000,
  PF_CTL_COMBO = 0x04000,
  PF_CTL_RADIO = 0x05000,
  
  PF_CTLOPTIONS = 0xFF0000,
  PF_CTLO_HORIZ = 0x010000,
  PF_CTLO_VERT = 0x020000,
  
  PF_UNITMASK = 0xFF000000,
  PF_UNIT_DB = 0x01000000,
  PF_UNIT_COEF = 0x02000000,
  PF_UNIT_HZ = 0x03000000,
  PF_UNIT_SEC = 0x04000000,
  PF_UNIT_MSEC = 0x05000000,
  PF_UNIT_CENTS = 0x06000000,
};

struct parameter_properties
{
    float def_value, min, max, step;
    uint32_t flags;
    const char **choices;
    float from_01(float value01) const;
    float to_01(float value) const;
    std::string to_string(float value) const;
};

struct midi_event {
    uint8_t command;
    uint8_t param1;
    uint16_t param2;
    float param3;
};

/// this is not a real class, just an empty example
class audio_module_iface
{
public:
    enum { in_count = 2, out_count = 2, param_count = 0, rt_capable = true };
    static parameter_properties *param_props;
    float **ins;
    float **outs;
    float **params;
    void set_sample_rate(uint32_t sr);
    void handle_event(uint8_t *data, int len) {}
    // all or most params changed
    void params_changed() {}
    uint32_t process_audio(uint32_t nsamples, uint32_t inputs_mask, uint32_t outputs_mask);
    static int get_in_channels();
    static int get_out_channels();
};

struct ladspa_info
{
    uint32_t unique_id;
    const char *label;
    const char *name;
    const char *maker;
    const char *copyright;
    const char *plugin_type;
};

#if USE_LADSPA

extern std::string generate_ladspa_rdf(const ladspa_info &info, parameter_properties *params, const char *param_names[], unsigned int count, unsigned int ctl_ofs);

template<class Module>
struct ladspa_wrapper
{
    LADSPA_Descriptor descriptor;
    ladspa_info &info;

    ladspa_wrapper(ladspa_info &i) 
    : info(i)
    {
        init_descriptor(i);
    }

    void init_descriptor(ladspa_info &i)
    {
        int ins = Module::in_count;
        int outs = Module::out_count;
        int params = Module::param_count;
        descriptor.UniqueID = i.unique_id;
        descriptor.Label = i.label;
        descriptor.Name = i.name;
        descriptor.Maker = i.maker;
        descriptor.Copyright = i.copyright;
        descriptor.Properties = LADSPA_PROPERTY_INPLACE_BROKEN | (Module::rt_capable ? LADSPA_PROPERTY_HARD_RT_CAPABLE : 0);
        descriptor.PortCount = ins + outs + params;
        descriptor.PortNames = Module::param_names;
        descriptor.PortDescriptors = new LADSPA_PortDescriptor[descriptor.PortCount];
        descriptor.PortRangeHints = new LADSPA_PortRangeHint[descriptor.PortCount];
        for (int i = 0; i < ins + outs + params; i++)
        {
            LADSPA_PortRangeHint &prh = ((LADSPA_PortRangeHint *)descriptor.PortRangeHints)[i];
            ((int *)descriptor.PortDescriptors)[i] = i < ins ? LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO
                                                  : i < ins + outs ? LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO
                                                                   : LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            if (i < ins + outs)
                prh.HintDescriptor = 0;
            else {            
                prh.HintDescriptor = LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_BOUNDED_BELOW;
                parameter_properties &pp = Module::param_props[i - ins - outs];
                prh.LowerBound = pp.min;
                prh.UpperBound = pp.max;
                switch(pp.flags & PF_TYPEMASK) {
                    case PF_BOOL: 
                        prh.HintDescriptor |= LADSPA_HINT_TOGGLED;
                        break;
                    case PF_INT: 
                    case PF_ENUM: 
                        prh.HintDescriptor |= LADSPA_HINT_INTEGER;
                        break;
                }
                switch(pp.flags & PF_SCALEMASK) {
                    case PF_SCALE_LOG:
                        prh.HintDescriptor |= LADSPA_HINT_LOGARITHMIC;
                        break;
                }
            }
        }
        descriptor.ImplementationData = this;
        descriptor.instantiate = cb_instantiate;
        descriptor.connect_port = cb_connect;
        descriptor.activate = cb_activate;
        descriptor.run = cb_run;
        descriptor.run_adding = NULL;
        descriptor.set_run_adding_gain = NULL;
        descriptor.deactivate = cb_deactivate;
        descriptor.cleanup = cb_cleanup;
    }

    static LADSPA_Handle cb_instantiate(const struct _LADSPA_Descriptor * Descriptor, unsigned long sample_rate)
    {
        Module *mod = new Module();
        for (int i=0; i<Module::in_count; i++)
            mod->ins[i] = NULL;
        for (int i=0; i<Module::out_count; i++)
            mod->outs[i] = NULL;
        for (int i=0; i<Module::param_count; i++)
            mod->params[i] = NULL;
        mod->srate = sample_rate;
        return mod;
    }

    static void cb_connect(LADSPA_Handle Instance, unsigned long port, LADSPA_Data *DataLocation) {
        unsigned long ins = Module::in_count;
        unsigned long outs = Module::out_count;
        unsigned long params = Module::param_count;
        Module *const mod = (Module *)Instance;
        if (port < ins)
            mod->ins[port] = DataLocation;
        else if (port < ins + outs)
            mod->outs[port - ins] = DataLocation;
        else if (port < ins + outs + params) {
            int i = port - ins - outs;
            mod->params[i] = DataLocation;
            *mod->params[i] = Module::param_props[i].def_value;
        }
    }

    static void cb_activate(LADSPA_Handle Instance) {
        Module *const mod = (Module *)Instance;
        mod->set_sample_rate(mod->srate);
        mod->activate();
    }

    static void cb_run(LADSPA_Handle Instance, unsigned long SampleCount) {
        Module *const mod = (Module *)Instance;
        mod->params_changed();
        uint32_t out_mask = mod->process(0, SampleCount, -1, -1);
        for (int i=0; i<Module::out_count; i++) {
          if ((out_mask & (1 << i)) == 0) {
            if (mod->outs[i])
              memset(mod->outs[i], 0, sizeof(float) * SampleCount);
          }
        }
    }

    static void cb_deactivate(LADSPA_Handle Instance) {
        Module *const mod = (Module *)Instance;
        mod->deactivate();
    }

    static void cb_cleanup(LADSPA_Handle Instance) {
        Module *const mod = (Module *)Instance;
        delete mod;
    }
    
    std::string generate_rdf() {
        return synth::generate_ladspa_rdf(info, Module::param_props, Module::param_names, Module::param_count, Module::in_count + Module::out_count);
    };
};

#endif

struct audio_exception: public std::exception
{
    const char *text;
    std::string container;
public:
    audio_exception(const char *_text) : text(_text) {}
    audio_exception(const std::string &t) : text(container.c_str()), container(t) {}
    virtual const char *what() { return text; }
    virtual ~audio_exception() throw () {}
};

};

#endif