// VowelWorm — MIDI note to harmony-voice assignment.
//
// Strategy (designed for keyboard-played chords against a live voice):
//  * A new note prefers a voice that was released recently (within the legato
//    window) and whose last note is nearest in pitch — so on a chord change
//    each voice glides the shortest distance instead of every voice jumping.
//  * If no recently released voice exists, the longest-idle free voice is used
//    and starts directly on pitch (no glide from a stale note).
//  * If all voices are busy, the oldest sustained-only (pedal) voice is stolen
//    first, then the oldest held voice; a stolen voice glides to its new note.
//  * Sustain pedal (CC64) defers releases; latch holds the chord until a new
//    note arrives with no keys physically down.
#pragma once

#include "Common.h"

namespace vw
{

struct VoiceSlot
{
    int   note = -1;          // currently assigned note (-1 = none)
    int   lastNote = -1;      // previous note (for legato reassignment)
    float velocity = 0.0f;
    bool  gate = false;       // sounding (held, sustained or latched)
    bool  keyDown = false;
    bool  sustained = false;
    bool  legato = false;     // true when the last assignment should glide
    int64_t onTime = 0, offTime = -1000000000;
    uint32_t triggerCount = 0; // increments on every (re)assignment
};

class VoiceAllocator
{
public:
    std::array<VoiceSlot, kMaxVoices> v {};
    int numVoices = 4;
    int64_t legatoWindow = 9600;  // samples (set from sample rate)
    bool latch = false;
    bool sustainDown = false;

    void reset() { v = {}; sustainDown = false; keysDown = 0; }

    void noteOn (int note, float vel, int64_t time) noexcept
    {
        // latch: a fresh chord (no keys down) replaces the latched one
        if (latch && keysDown == 0)
            for (int i = 0; i < numVoices; ++i)
                if (v[(size_t) i].gate && ! v[(size_t) i].keyDown) release (i, time);

        ++keysDown;

        for (int i = 0; i < numVoices; ++i)
            if (v[(size_t) i].note == note && v[(size_t) i].gate)
            {
                auto& s = v[(size_t) i];
                s.keyDown = true; s.sustained = false; s.velocity = vel;
                s.onTime = time; ++s.triggerCount;
                return;
            }

        int best = -1; float bestCost = 1.0e9f; bool legato = false;
        for (int i = 0; i < numVoices; ++i)
        {
            const auto& s = v[(size_t) i];
            if (s.gate) continue;
            const bool recent = s.lastNote >= 0 && (time - s.offTime) < legatoWindow;
            const float cost = recent ? (float) std::abs (s.lastNote - note)
                                      : 1000.0f - (float) std::min<int64_t> (time - s.offTime, 1000000) * 1.0e-4f;
            if (cost < bestCost) { bestCost = cost; best = i; legato = recent; }
        }

        if (best < 0)
        {
            // steal: oldest pedal-only voice first, otherwise the oldest voice
            int64_t oldest = INT64_MAX;
            for (int pass = 0; pass < 2 && best < 0; ++pass)
                for (int i = 0; i < numVoices; ++i)
                {
                    const auto& s = v[(size_t) i];
                    if (pass == 0 && s.keyDown) continue;
                    if (s.onTime < oldest) { oldest = s.onTime; best = i; }
                }
            legato = true;
        }

        auto& s = v[(size_t) best];
        s.lastNote = s.note >= 0 ? s.note : s.lastNote;
        s.note = note; s.velocity = vel; s.gate = true; s.keyDown = true; s.sustained = false;
        s.legato = legato && s.lastNote >= 0;
        s.onTime = time; ++s.triggerCount;
    }

    void noteOff (int note, int64_t time) noexcept
    {
        keysDown = std::max (0, keysDown - 1);
        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& s = v[(size_t) i];
            if (s.note != note || ! s.keyDown) continue;
            s.keyDown = false;
            if (latch) continue;
            if (sustainDown) { s.sustained = true; continue; }
            release (i, time);
        }
    }

    void setSustain (bool down, int64_t time) noexcept
    {
        sustainDown = down;
        if (! down)
            for (int i = 0; i < kMaxVoices; ++i)
                if (v[(size_t) i].sustained && ! v[(size_t) i].keyDown) release (i, time);
    }

    void setLatch (bool on, int64_t time) noexcept
    {
        if (latch == on) return;
        latch = on;
        if (! on)
            for (int i = 0; i < kMaxVoices; ++i)
                if (v[(size_t) i].gate && ! v[(size_t) i].keyDown && ! sustainDown) release (i, time);
    }

    void allNotesOff (int64_t time) noexcept
    {
        for (int i = 0; i < kMaxVoices; ++i) if (v[(size_t) i].gate) release (i, time);
        for (auto& s : v) { s.keyDown = false; s.sustained = false; }
        keysDown = 0;
    }

    /** Called when the active voice count shrinks: release voices that are now out of range. */
    void setNumVoices (int n, int64_t time) noexcept
    {
        n = std::max (1, std::min (kMaxVoices, n));
        for (int i = n; i < kMaxVoices; ++i) if (v[(size_t) i].gate) release (i, time);
        numVoices = n;
    }

    int keysHeld() const noexcept { return keysDown; }

private:
    void release (int i, int64_t time) noexcept
    {
        auto& s = v[(size_t) i];
        s.gate = false; s.sustained = false;
        s.lastNote = s.note;
        s.offTime = time;
        // note stays assigned so the release tail keeps its pitch
    }

    int keysDown = 0;
};

} // namespace vw
