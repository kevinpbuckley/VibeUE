# Animation Curves Management Tests

Test prompts for working with animation curves.

---

## 01 - List Existing Curves

List all curves in an animation sequence. Show:
- Curve name
- Curve type
- Key count
- Whether it drives a morph target

---

## 02 - Get Curve Info

Get detailed information about a specific curve by name.

---

## 03 - Get Curve Value at Time

Get the value of a curve at time 0.5 seconds.

---

## 04 - Get Curve Keyframes

Get all keyframes for a specific curve. Show time, value, and interpolation mode for each.

---

## 05 - Add New Curve

Add a new float curve named "BlendWeight" to an animation.

---

## 06 - Add Curve Key

Add a key to the "BlendWeight" curve at time 0.5 with value 1.0.

---

## 07 - Set Multiple Curve Keys

Set the "BlendWeight" curve with these keys:
- Time 0.0, Value 0.0
- Time 0.25, Value 0.5
- Time 0.5, Value 1.0
- Time 0.75, Value 0.5
- Time 1.0, Value 0.0

---

## 08 - Remove Curve

Remove the "BlendWeight" curve from the animation. Verify by listing curves.

---

## 09 - Create Morph Target Curve

Add a curve named "Smile" that will drive a morph target. Set it to peak at the midpoint of the animation.

---

## 10 - Evaluate Curve at Multiple Points

Sample a curve at 10 evenly spaced points and plot the values (text output showing the curve shape).
