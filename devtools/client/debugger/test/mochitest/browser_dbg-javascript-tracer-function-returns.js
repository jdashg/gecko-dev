/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing all function returns

"use strict";

add_task(async function testTracingFunctionReturn() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const jsCode = `async function foo() { nullReturn(); falseReturn(); await new Promise(r => setTimeout(r, 0)); return bar(); }; function nullReturn() { return null; } function falseReturn() { return false; } function bar() { return 42; }; function throwingFunction() { throw new Error("the exception") }`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," +
      encodeURIComponent(`<script>${jsCode}</script><body></body>`)
  );

  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-function-return");

  await toggleJsTracer(dbg.toolbox);

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");
  await hasConsoleMessage(dbg, "⟶ interpreter λ foo");
  await hasConsoleMessage(dbg, "⟶ interpreter λ bar");
  await hasConsoleMessage(dbg, "⟵ λ bar");
  await hasConsoleMessage(dbg, "⟵ λ foo");

  await toggleJsTracer(dbg.toolbox);

  info("Wait for tracing to be disabled");
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");

  await toggleJsTracer(dbg.toolbox);

  info("Wait for tracing to be re-enabled with logging of returned values");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "⟶ interpreter λ foo");
  await hasConsoleMessage(dbg, "⟶ interpreter λ bar");
  await hasConsoleMessage(dbg, "⟵ λ bar return 42");
  await hasConsoleMessage(dbg, "⟶ interpreter λ nullReturn");
  await hasConsoleMessage(dbg, "⟵ λ nullReturn return null");
  await hasConsoleMessage(dbg, "⟶ interpreter λ falseReturn");
  await hasConsoleMessage(dbg, "⟵ λ falseReturn return false");
  await hasConsoleMessage(
    dbg,
    `⟵ λ foo return \nPromise { <state>: "fulfilled", <value>: 42 }`
  );

  invokeInTab("throwingFunction").catch(() => {});
  await hasConsoleMessage(
    dbg,
    `⟵ λ throwingFunction throw \nError: the exception`
  );

  info("Stop tracing");
  await toggleJsTracer(dbg.toolbox);
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  info("Toggle the two settings to the default value");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-function-return");

  // Reset the trace on next interaction setting
  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-on-next-interaction"
  );
});
