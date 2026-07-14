(function () {
  "use strict";

  function postJSON(url, body) {
    return fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(Object.assign({ csrf_token: window.CSRF_TOKEN }, body || {})),
    }).then((res) => {
      if (!res.ok) {
        return res.text().then((text) => {
          throw new Error(text || res.statusText);
        });
      }
      return res.json();
    });
  }

  function applySnapshot(snapshot) {
    const badge = document.getElementById("availability-badge");
    if (badge) {
      badge.className = "badge badge-" + snapshot.availability;
      badge.textContent =
        snapshot.availability === "online"
          ? "● Online"
          : snapshot.availability === "offline"
          ? "○ Offline"
          : "○ Unknown";
    }

    const status = snapshot.status;
    if (!status) return;

    const leakBanner = document.getElementById("leak-banner");
    if (leakBanner) {
      leakBanner.style.display = status.leak_lockout ? "block" : "none";
    }

    (status.outputs || []).forEach((entry) => {
      const card = document.querySelector('[data-output="' + entry.name + '"]');
      if (!card) return;
      const stateBadge = card.querySelector(".state-badge");
      if (!stateBadge) return;
      if (entry.name === "float1") {
        // state === true means the float is out of the water (level LOW, fill needed).
        stateBadge.textContent = entry.state ? "LOW" : "OK";
        stateBadge.className = "badge state-badge " + (entry.state ? "badge-warn" : "badge-on");
      } else {
        stateBadge.textContent = entry.state ? "ON" : "OFF";
        stateBadge.className = "badge state-badge " + (entry.state ? "badge-on" : "badge-off");
      }
    });
  }

  function connectEvents() {
    const source = new EventSource("/events");
    source.onmessage = (evt) => {
      try {
        applySnapshot(JSON.parse(evt.data));
      } catch (err) {
        console.error("bad event payload", err);
      }
    };
    source.onerror = () => {
      // EventSource retries automatically; nothing to do here.
    };
  }

  function withDisabled(button, fn) {
    button.disabled = true;
    Promise.resolve()
      .then(fn)
      .catch((err) => alert("Action failed: " + err.message))
      .finally(() => {
        button.disabled = false;
      });
  }

  document.addEventListener("DOMContentLoaded", () => {
    if (document.getElementById("availability-badge")) {
      connectEvents();
    }

    const refreshBtn = document.getElementById("btn-refresh");
    if (refreshBtn) {
      refreshBtn.addEventListener("click", () =>
        withDisabled(refreshBtn, () => postJSON("/cmd", { command: "request_status" }))
      );
    }

    const resendBtn = document.getElementById("btn-resend-config");
    if (resendBtn) {
      resendBtn.addEventListener("click", () =>
        withDisabled(resendBtn, () => postJSON("/config/resend", {}))
      );
    }

    const rebootBtn = document.getElementById("btn-reboot");
    if (rebootBtn) {
      rebootBtn.addEventListener("click", () => {
        if (!confirm("Reboot the device now? It will drop offline briefly.")) return;
        withDisabled(rebootBtn, () => postJSON("/cmd", { command: "reboot" }));
      });
    }

    const clearLockoutBtn = document.getElementById("btn-clear-lockout");
    if (clearLockoutBtn) {
      clearLockoutBtn.addEventListener("click", () =>
        withDisabled(clearLockoutBtn, () => postJSON("/cmd", { command: "clear_leak_lockout" }))
      );
    }

    document.querySelectorAll(".card[data-output]").forEach((card) => {
      const name = card.getAttribute("data-output");
      card.querySelectorAll("[data-override]").forEach((btn) => {
        btn.addEventListener("click", () =>
          withDisabled(btn, () =>
            postJSON("/override", { name: name, override: parseInt(btn.dataset.override, 10) })
          )
        );
      });
    });
  });
})();
