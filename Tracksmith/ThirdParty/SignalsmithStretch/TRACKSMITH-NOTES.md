# Pinned Time Warp dependencies

- Signalsmith Stretch: https://github.com/Signalsmith-Audio/signalsmith-stretch — commit `a670068d9aeb64913331d5cc29337b19a457a7df`.
- Signalsmith Linear: https://github.com/Signalsmith-Audio/linear — commit `146b26f91b48835d665fa0498d64d9617818d27a`.

Both are MIT licensed. Original license files are retained in this folder and signalsmith-linear/. The vendored headers are unchanged. Tracksmith's rendering wrapper is in source/ElasticRender.h; generated-media fingerprints include the algorithm/dependency versions.

The feature is presented to users as **Time Warp**. Internal `elastic` identifiers describe the timing implementation and are not the product name.
