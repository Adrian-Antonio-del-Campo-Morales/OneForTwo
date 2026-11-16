# Distribution for Ubuntu

Status: pending; no package or installation scripts. Responsibility: Host the native package
definition for verified Ubuntu versions.

You must install the executable and resources, register the launcher, declare dependencies actually
used and allow optional booting when you start the graphical session. The agent will function as a
user; a root service is not assumed.

You will document required desktop, session and portals, as well as clipboard and input permissions
and limitations. Access to the configuration will not depend on having a tray icon.

Before publishing: fix versiones/arquitecturas, package format, update and clean. If an isolated
package is evaluated, recheck access to files and desktop APIs with that package.

