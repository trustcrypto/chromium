// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-password-screen',

  properties: {
    invalid: Boolean,

    strings: Object,
  },

  get active() {
    return this.$.dialog.open;
  },

  show: function() {
    this.$.dialog.showModal();
  },

  close: function() {
    if (this.active)
      this.$.dialog.close();
  },

  deny: function() {
    const password = /** @type {!CrInputElement} */ (this.$.password);
    password.disabled = false;
    this.$.submit.disabled = false;
    this.invalid = true;
    password.select();
  },

  submit: function() {
    const password = /** @type {!CrInputElement} */ (this.$.password);
    if (password.value.length == 0)
      return;
    password.disabled = true;
    this.$.submit.disabled = true;
    this.fire('password-submitted', {password: password.value});
  },
});
