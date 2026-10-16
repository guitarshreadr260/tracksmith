# Tracksmith Deesser development

Original selective band-cancellation de-esser with manual frequency, reduction cap, auto/manual threshold, adjustable release and band listen. A bandpass component is subtracted dynamically from the unfiltered signal. Auto mode compares band and broadband levels. This does not clone SPL circuitry or automatically scan for a centre frequency. Reference: https://spl.audio/wp-content/uploads/9629_OM_E.pdf . Frequency is clamped below Nyquist for low sample rates.

Previews and automated tests only; no new DAW installer or portable package has been prepared. Listening and broader transient/automation measurements remain.
