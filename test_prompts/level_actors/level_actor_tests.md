# Level Actor Tests

Tests for placing, transforming, and managing actors in the level. Run sequentially.

---

## Finding Actors

What actors are currently in the level?

---

Show me just the lights.

---

Are there any actors tagged with "Lighting"?

---

List the static meshes but cap it at 20 results.

---

## Creating Point Lights

Put a point light at location 500, 500, 300 and call it TestLight1.

---

Tell me about that light.

---

Add another point light at 800, 500, 300 called TestLight2. Tag it with "Test" and "Lighting".

---

Find all actors starting with "TestLight".

---

## Creating a Spotlight

Add a spotlight at the origin but 500 units up, angled down at 45 degrees. Name it TestSpotLight.

---

Show me the transform on that spotlight.

---

Verify the rotation is correct.

---

## Moving Actors

What's the current position of TestLight1?

---

Move it to 1000, 1000, 400.

---

Check that the move worked.

---

Slide it over to 1200, 1000, 400 and do a sweep to check for collisions.

---

## Rotating Actors

Point the spotlight straight down (90 degree pitch).

---

Verify the rotation.

---

Now tilt it slightly - 90 pitch and 45 yaw.

---

Try a relative rotation in local space.

---

## Scaling Actors

Put a cube mesh at the origin, 100 units up. Call it TestCube.

---

Double its size in all dimensions.

---

Make it stretched - 1x in X, 2x in Y, half in Z.

---

Check the scale values.

---

## Full Transform

Set the cube to location 500, 500, 200, rotation 45 degrees around Y, and scale 1.5 uniformly - all at once.

---

Verify the transform.

---

Reset it back to default transform.

---

## Light Properties

What's the intensity on TestLight1?

---

Crank it up to 10000.

---

Verify the change.

---

Make it a warm color.

---

Show me all the properties on that light.

---

## Organizing Actors

Put TestLight1 in an outliner folder called TestLights.

---

Attach TestLight2 to the cube so it follows along.

---

Now detach TestLight2 so it's independent again.

---

Select TestLight1 in the editor.

---

## Editor Operations

Focus the camera on the spotlight.

---

Move the cube to the current viewport location.

---

Refresh the viewport.

---

Rename TestLight1 to MainLight.

---

## Cleanup

Delete all the test actors we created - both test lights, the spotlight, and the cube. Force delete without confirmation.
