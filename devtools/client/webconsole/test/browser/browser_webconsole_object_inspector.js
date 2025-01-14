/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object inspector in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test Object Inspector</h1>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  logAllStoreChanges(hud);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.log("oi-test", [1, 2, { a: "a", b: "b" }], {
      c: "c",
      d: [3, 4],
      length: 987,
    });
  });

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    2,
    "There is the expected number of object inspectors"
  );

  const [arrayOi, objectOi] = objectInspectors;

  let arrayOiArrowButton = arrayOi.querySelector(".theme-twisty");
  is(
    arrayOiArrowButton.getAttribute("title"),
    "Expand",
    "Toggle button has expected title when node is collapsed"
  );

  info("Expanding the array object inspector");

  let onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });
  arrayOiArrowButton.click();
  await onArrayOiMutation;

  arrayOiArrowButton = arrayOi.querySelector(".theme-twisty");
  ok(
    arrayOi.querySelector(".theme-twisty").classList.contains("open"),
    "Toggle button of the root node of the tree is expanded after clicking on it"
  );
  is(
    arrayOiArrowButton.getAttribute("title"),
    "Collapse",
    "Toggle button has expected title when node is expanded"
  );

  let arrayOiNodes = arrayOi.querySelectorAll(".node");

  // The object inspector now looks like:
  // ▼ […]
  // |  0: 1
  // |  1: 2
  // |  ▶︎ 2: {a: "a", b: "b"}
  // |  length: 3
  // |  ▶︎ <prototype>
  is(
    arrayOiNodes.length,
    6,
    "There is the expected number of nodes in the tree"
  );

  info("Expanding a leaf of the array object inspector");
  let arrayOiNestedObject = arrayOiNodes[3];
  onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });

  arrayOiNestedObject.querySelector(".theme-twisty").click();
  await onArrayOiMutation;

  ok(
    arrayOiNestedObject
      .querySelector(".theme-twisty")
      .classList.contains("open"),
    "The arrow of the root node of the tree is expanded after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");

  // The object inspector now looks like:
  // ▼ […]
  // |  0: 1
  // |  1: 2
  // |  ▼ 2: {…}
  // |  |  a: "a"
  // |  |  b: "b"
  // |  |  ▶︎ <prototype>
  // |  length: 3
  // |  ▶︎ <prototype>
  is(
    arrayOiNodes.length,
    9,
    "There is the expected number of nodes in the tree"
  );

  info("Collapsing the root");
  onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });
  arrayOi.querySelector(".theme-twisty").click();

  is(
    arrayOi.querySelector(".theme-twisty").classList.contains("open"),
    false,
    "The arrow of the root node of the tree is collapsed after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");
  is(arrayOiNodes.length, 1, "Only the root node is visible");

  info("Expanding the root again");
  onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });
  arrayOi.querySelector(".theme-twisty").click();

  ok(
    arrayOi.querySelector(".theme-twisty").classList.contains("open"),
    "The arrow of the root node of the tree is expanded again after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");
  arrayOiNestedObject = arrayOiNodes[3];
  ok(
    arrayOiNestedObject
      .querySelector(".theme-twisty")
      .classList.contains("open"),
    "The object tree is still expanded"
  );

  is(
    arrayOiNodes.length,
    9,
    "There is the expected number of nodes in the tree"
  );

  const onObjectOiMutation = waitForNodeMutation(objectOi, {
    childList: true,
  });

  objectOi.querySelector(".theme-twisty").click();
  await onObjectOiMutation;

  ok(
    objectOi.querySelector(".theme-twisty").classList.contains("open"),
    "The arrow of the root node of the tree is expanded after clicking on it"
  );

  const objectOiNodes = objectOi.querySelectorAll(".node");
  // The object inspector now looks like:
  // ▼ {…}
  // |  c: "c"
  // |  ▶︎ d: [3, 4]
  // |  length: 987
  // |  ▶︎ <prototype>
  is(
    objectOiNodes.length,
    5,
    "There is the expected number of nodes in the tree"
  );
});
