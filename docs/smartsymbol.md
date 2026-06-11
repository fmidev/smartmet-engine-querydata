# SmartSymbol

SmartSymbol is a derived weather parameter computed from several NWP model fields. It produces an integer code describing the dominant weather condition at a location and time. Day values are 1–77; **100 is added** when solar position indicates darkness, giving night values 101–177.

> [!NOTE]
> This document covers SmartSymbol as derived from gridded forecast data in the QEngine. SmartSymbol can also be derived from observations, but that is handled by a separate codebase and is out of scope here.

## Input parameters

| Parameter | FMI ID | Notes |
| --- | --- | --- |
| Total cloud cover (%) | `kFmiTotalCloudCover` | Always required |
| Probability of thunderstorm (%) | `kFmiProbabilityThunderstorm` | Optional; triggers thunder branch when ≥ 30 % |
| 1-hour precipitation (mm) | `kFmiPrecipitation1h` | Required when no thunder |
| Precipitation form | `kFmiPotentialPrecipitationForm` / `kFmiPrecipitationForm` | Required when precipitation ≥ 0.025 mm |
| Precipitation type | `kFmiPotentialPrecipitationType` / `kFmiPrecipitationType` | Required for liquid water branch |
| Fog intensity | `kFmiFogIntensity` | Optional; triggers fog symbol when > 0 |

### Precipitation form codes

| Code | Meaning |
| --- | --- |
| 0 | Drizzle |
| 1 | Water (rain) |
| 2 | Sleet |
| 3 | Snow |
| 4 | Freezing drizzle |
| 5 | Freezing rain |
| 6 | Hail |
| 7 | Snow pellets |
| 8 | Ice particles |

### Precipitation type codes

| Code | Meaning |
| --- | --- |
| 1 | Large-scale (stratiform) |
| 2 | Convective (shower) |

## Calculation logic

The algorithm evaluates conditions in strict priority order and returns as soon as a branch matches:

1. **Thunder** — if POT ≥ 30 %, return a thunderstorm symbol immediately (no precipitation form considered).
2. **No precipitation** — if rain < 0.025 mm/h, check fog intensity, then cloud cover.
3. **Precipitation form** — select the sub-branch: drizzle, freezing drizzle, freezing rain, snow pellets/ice, hail, rain, sleet, or snow.
4. **Precipitation type** — for liquid rain only: convective (showers) vs. large-scale (stratiform).
5. **Day/night** — add 100 to any base symbol when solar position is dark.

## Symbol value table

> Night variants: add 100 to every value (e.g. day **31** → night **131**).

### Clear / cloudy / fog (no precipitation, rain < 0.025 mm/h)

| Symbol | Condition |
| --- | --- |
| **1** | Clear — cloud cover < 20 % |
| **2** | Mostly clear — 20–33 % |
| **4** | Partly cloudy — 33–72 % |
| **6** | Mostly cloudy — 72–93 % |
| **7** | Overcast — ≥ 93 % |
| **9** | Fog — fog intensity > 0 (any cloud cover) |

### Drizzle and freezing precipitation (fixed — no cloudiness or intensity variation)

| Symbol | Precipitation form |
| --- | --- |
| **11** | Drizzle (form = 0) |
| **14** | Freezing drizzle (form = 4) |
| **17** | Freezing rain (form = 5) |

### Showers — convective rain (form = 1, type = 2)

| Symbol | Cloud cover |
| --- | --- |
| **21** | < 72 % |
| **24** | 72–93 % |
| **27** | ≥ 93 % |

### Large-scale rain (form = 1, type = 1 or missing)

Intensity thresholds: light < 0.4 mm/h, moderate 0.4–4 mm/h, heavy ≥ 4 mm/h.

| Symbol | Cloud cover | Precipitation (mm/h) |
| --- | --- | --- |
| **31** | < 72 % | < 0.4 |
| **32** | < 72 % | 0.4–4 |
| **33** | < 72 % | ≥ 4 |
| **34** | 72–93 % | < 0.4 |
| **35** | 72–93 % | 0.4–4 |
| **36** | 72–93 % | ≥ 4 |
| **37** | ≥ 93 % | < 0.4 |
| **38** | ≥ 93 % | 0.4–4 |
| **39** | ≥ 93 % | ≥ 4 |

### Sleet (form = 2)

Intensity thresholds: light < 0.4 mm/h, moderate 0.4–1.5 mm/h, heavy ≥ 1.5 mm/h.

| Symbol | Cloud cover | Precipitation (mm/h) |
| --- | --- | --- |
| **41** | < 72 % | < 0.4 |
| **42** | < 72 % | 0.4–1.5 |
| **43** | < 72 % | ≥ 1.5 |
| **44** | 72–93 % | < 0.4 |
| **45** | 72–93 % | 0.4–1.5 |
| **46** | 72–93 % | ≥ 1.5 |
| **47** | ≥ 93 % | < 0.4 |
| **48** | ≥ 93 % | 0.4–1.5 |
| **49** | ≥ 93 % | ≥ 1.5 |

### Snow (form = 3)

Same intensity thresholds as sleet: light < 0.4 mm/h, moderate 0.4–1.5 mm/h, heavy ≥ 1.5 mm/h.

| Symbol | Cloud cover | Precipitation (mm/h) |
| --- | --- | --- |
| **51** | < 72 % | < 0.4 |
| **52** | < 72 % | 0.4–1.5 |
| **53** | < 72 % | ≥ 1.5 |
| **54** | 72–93 % | < 0.4 |
| **55** | 72–93 % | 0.4–1.5 |
| **56** | 72–93 % | ≥ 1.5 |
| **57** | ≥ 93 % | < 0.4 |
| **58** | ≥ 93 % | 0.4–1.5 |
| **59** | ≥ 93 % | ≥ 1.5 |

> Note: symbol 57 is shared with the snow-pellets/ice-particles fallback (form 7 or 8 → 57 directly). Both resolve to the same meaning: overcast snowfall.

### Hail (form = 6)

| Symbol | Cloud cover |
| --- | --- |
| **61** | < 72 % |
| **64** | 72–93 % |
| **67** | ≥ 93 % |

### Thunder (POT ≥ 30 %)

Checked before precipitation form; overrides all other branches.

| Symbol | Cloud cover |
| --- | --- |
| **71** | < 72 % |
| **74** | 72–93 % |
| **77** | ≥ 93 % |

## Troubleshooting

### SmartSymbol is missing (no value returned)

The function returns missing if any required input is absent or its interpolated value is `kFloatMissing`. Check in order:

1. `kFmiTotalCloudCover` is always required — missing or missing-valued → None.
2. `kFmiPrecipitation1h` is required unless POT ≥ 30 % — missing or missing-valued → None.
3. When rain ≥ 0.025 mm/h, both `kFmiPotentialPrecipitationForm` and `kFmiPrecipitationForm` are absent → None.

### Symbol is always overcast (7, 37, 38, 39, 57–59, 74, 77, …)

If cloud cover is unexpectedly treated as overcast everywhere, the interpolated `kFmiTotalCloudCover` value may be IEEE NaN rather than `kFloatMissing`. NaN passes the `== kFloatMissing` guard and then fails every `< threshold` comparison, making the code behave as though cloud cover is above all limits. See the edge cases section for details.

### Getting drizzle (11) unexpectedly when precipitation form should be something else

When `kFmiPotentialPrecipitationForm` / `kFmiPrecipitationForm` returns IEEE NaN, casting it to `int` is undefined behaviour. On most x86 platforms this produces 0, which is the drizzle code, so symbol 11 is returned instead of None.

### Thunder symbols (71–77) with no apparent storm

The POT threshold is only 30 %. A relatively low forecast probability of thunderstorms is enough to trigger the thunder branch, which overrides precipitation form entirely. Inspect the `kFmiProbabilityThunderstorm` value at the location and time in question.

### Correct precipitation type but wrong intensity level

Check the actual `kFmiPrecipitation1h` value against the thresholds for the form in question:

- Rain (form = 1): light < 0.4 mm/h, moderate 0.4–4 mm/h, heavy ≥ 4 mm/h
- Sleet (form = 2) and snow (form = 3): light < 0.4 mm/h, moderate 0.4–1.5 mm/h, heavy ≥ 1.5 mm/h

### Showers (21–27) when large-scale rain (31–39) is expected, or vice versa

Check `kFmiPotentialPrecipitationType` / `kFmiPrecipitationType`: 2 = convective (showers), 1 = large-scale. If the parameter is absent the code defaults to large-scale.

### Symbol value above 77 during daytime

If a daytime symbol exceeds 77 but is not in the expected night range (101–177), a precipitation form code outside 0–8 has reached the catch-all formula `10 × form + 21 + …` and produced an out-of-range value. See the edge cases section.

## Edge cases

### SmartSymbol cannot be IEEE NaN

`calc_smart_symbol()` returns `std::optional<int>`. Every code path either returns `{}` (which becomes `TS::None()` / missing in `SmartSymbolNumber()`) or an integer produced by arithmetic on small constant values. The day/night addition `100 + *symbol` is also integer-only. The output is therefore always either a concrete integer or missing — never a floating-point NaN.

### Only `kFloatMissing` is guarded, not IEEE NaN

All missing-value checks use `== kFloatMissing`. If `q.interpolate()` returns an IEEE NaN (possible when underlying grid data is corrupt or improperly initialized), the NaN passes through every `== kFloatMissing` guard silently:

| Variable | What happens with NaN |
| --- | --- |
| `n` (cloud cover) | All `n < limit` comparisons return false → treated as overcast (≥ 93 %) in every branch |
| `rain` | `rain < 0.025` returns false → precipitation branch entered even though no valid rain value exists |
| `rform` | `static_cast<int>(NaN)` is **undefined behaviour**; on most platforms yields 0 (drizzle) or `INT_MIN`, silently producing a wrong symbol instead of None |
| `rtype` | Same UB issue; if the cast accidentally produces 2 the convective branch fires incorrectly |
| `fog` | `NaN > 0` returns false → fog branch safely skipped |
| `thunder` | `NaN >= 30` returns false → thunder branch safely skipped |

### `kFloatMissing` for `rtype` silently falls through to large-scale

When the precipitation type parameter exists but its interpolated value is `kFloatMissing` (≈ 32700), there is no explicit guard. `static_cast<int>(kFloatMissing)` ≈ 32700, which is not equal to 2 (convective), so the code falls through to the large-scale rain branch. This happens to be the correct default but relies on `kFloatMissing` not coincidentally equalling 2.

### Unrecognized `rform` values produce out-of-range symbols

Form codes 0–8 are all handled explicitly. Any other value reaches the catch-all formula `10 * rform + 21 + …`. For `rform` ≥ 9 this produces values ≥ 112, which overlap the night range (100+) and would be misinterpreted as nighttime variants of existing symbols. There is no guard that returns missing for unknown form codes.

### `thunder` condition order

The thunder check is written as `thunder >= thunder_limit1 && thunder != kFloatMissing`. Because `kFloatMissing` ≈ 32700 satisfies `>= 30`, the first sub-expression is true for missing values. The second sub-expression saves it. Swapping the order would be safer and more idiomatic, but the current order is not a bug in practice.

## Source

`querydata/Q.cpp`, function `calc_smart_symbol()` (lines 767–883) and `SmartSymbolNumber()` (lines 1014–1037).
