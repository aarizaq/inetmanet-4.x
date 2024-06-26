.. _ug:cha:visualization:

Visualization
=============

.. _ug:sec:visualization:overview:

Overview
--------

The INET Framework is capable of visualizing a wide range of events and
conditions in the network: packet drops, data link connectivity, wireless signal
path loss, transport connections, routing table routes, and many more.
Visualization is implemented as a collection of configurable INET modules that
can be added to simulations at will.

.. _ug:sec:visualization:network-communication:

Visualizing Network Communication
---------------------------------

.. _ug:sec:visualization:packet-drops:

Visualizing Packet Drops
~~~~~~~~~~~~~~~~~~~~~~~~

Several network problems manifest themselves as excessive packet drops, such as
poor connectivity, congestion, or misconfiguration. Visualizing packet drops
helps in identifying such problems in simulations and reduces time spent on
debugging and analysis. Poor connectivity in a wireless network can cause
senders to drop unacknowledged packets after the retry limit is exceeded.
Congestion can cause queues to overflow in a bottleneck router, resulting in
packet drops.

Packet drops can be visualized by including a :ned:`PacketDropVisualizer` module
in the simulation. The :ned:`PacketDropVisualizer` module indicates packet drops
by displaying an animation effect at the node where the packet drop occurs. In
the animation, a packet icon gets thrown out from the node icon and fades away.

The visualization of packet drops can be enabled with the visualizer's
:par:`displayPacketDrops` parameter. By default, packet drops at all nodes are
visualized. This selection can be narrowed with the :par:`nodeFilter`,
:par:`interfaceFilter`, and :par:`packetFilter` parameters.

One can click on the packet drop icon to display information about the packet
drop in the inspector panel.

Packets are dropped for the following reasons:

- Queue overflow

- Retry limit exceeded

- Unroutable packet

- Network address resolution failed

- Interface down

.. _ug:sec:visualization:transport-path-activity:

Visualizing Transport Path Activity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With INET simulations, it is often useful to be able to visualize
network traffic. INET provides several visualizers for this task,
operating at various levels of the network stack. One of such
visualizers is :ned:`TransportRouteVisualizer` that provides graphical
feedback about transport layer traffic.

:ned:`TransportRouteVisualizer` visualizes traffic that passes through
the transport layers of two endpoints. Adding an
:ned:`IntegratedVisualizer` is also an option, because it also contains
a :ned:`TransportRouteVisualizer`. Transport path activity visualization
is disabled by default, it can be enabled by setting the visualizer's
:par:`displayRoutes` parameter to true.

:ned:`TransportRouteVisualizer` observes packets that pass through the
transport layer, i.e. carry data from/to higher layers.

The activity between two nodes is represented visually by a polyline
arrow which points from the source node to the destination node.
:ned:`TransportRouteVisualizer` follows packets throughout their path so
that the polyline goes through all nodes which are the part of the path
of packets. The arrow appears after the first packet has been received,
then gradually fades out unless it is reinforced by further packets.
Color, fading time and other graphical properties can be changed with
parameters of the visualizer.

By default, all packets and nodes are considered for the visualization.
This selection can be narrowed with the visualizer's packetFilter and
nodeFilter parameters.

.. _ug:sec:visualization:network-path-activity:

Visualizing Network Path Activity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Network layer traffic can be visualized by including a
:ned:`NetworkRouteVisualizer` module in the simulation. Adding an
:ned:`IntegratedVisualizer` module is also an option, because it also
contains a :ned:`NetworkRouteVisualizer` module. Network path activity
visualization is disabled by default, it can be enabled by setting the
visualizer's :par:`displayRoutes` parameter to true.

:ned:`NetworkRouteVisualizer` currently observes packets that pass
through the network layer (i.e. carry data from/to higher layers), but
not those that are internal to the operation of the network layer
protocol. That is, packets such as ARP, although potentially useful,
will not trigger the visualization.

The activity between two nodes is represented visually by a polyline
arrow which points from the source node to the destination node.
:ned:`NetworkRouteVisualizer` follows packet throughout its path so the
polyline goes through all nodes that are part of the packet's path. The
arrow appears after the first packet has been received, then gradually
fades out unless it is reinforced by further packets. Color, fading time
and other graphical properties can be changed with parameters of the
visualizer.

By default, all packets and nodes are considered for the visualization.
This selection can be narrowed with the visualizer's packetFilter and
nodeFilter parameters.

.. _ug:sec:visualization:data-link-activity:

Visualizing Data Link Activity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Data link activity (layer 2 traffic) can be visualized by adding a
:ned:`DataLinkVisualizer` module to the simulation. Adding an
:ned:`IntegratedVisualizer` module is also an option, because it
includes a :ned:`DataLinkVisualizer` module. Data link visualization is
disabled by default, it can be enabled by setting the visualizer's
displayLinks parameter to true.

:ned:`DataLinkVisualizer` currently observes packets that pass through
the data link layer (i.e. carry data from/to higher layers), but not
those that are internal to the operation of the data link layer
protocol. That is, frames such as ACK, RTS/CTS, Beacon or
Authentication/Association frames of IEEE 802.11, although potentially
useful, will not trigger the visualization. Visualizing such frames may
be implemented in future INET revisions.

The activity between two nodes is represented visually by an arrow that
points from the sender node to the receiver node. The arrow appears
after the first packet has been received, then gradually fades out
unless it is refreshed by further packets. The style, color, fading time
and other graphical properties can be changed with parameters of the
visualizer.

By default, all packets, interfaces and nodes are considered for the
visualization. This selection can be narrowed to certain packets and/or
nodes with the visualizer's :par:`packetFilter`, :par:`interfaceFilter`,
and :par:`nodeFilter` parameters.

.. _ug:sec:visualization:physical-link-activity:

Visualizing Physical Link Activity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Physical link activity can be visualized by including a
:ned:`PhysicalLinkVisualizer` module in the simulation. Adding an
:ned:`IntegratedVisualizer` module is also an option, as it also contains a
:ned:`PhysicalLinkVisualizer` module. Physical link activity visualization is
disabled by default, but it can be enabled by setting the visualizer's
:par:`displayLinks` parameter to true.

:ned:`PhysicalLinkVisualizer` observes frames that pass through the physical
layer, meaning they are received correctly.

The activity between two nodes is visually represented by a dotted arrow that
points from the sender node to the receiver node. The arrow appears after the
first frame has been received and gradually fades out unless it is refreshed by
further frames. Color, fading time, and other graphical properties can be
changed with parameters of the visualizer.

By default, all packets, interfaces, and nodes are considered for the
visualization. This selection can be narrowed with the visualizer's
:par:`packetFilter`, :par:`interfaceFilter`, and :par:`nodeFilter` parameters.

.. _ug:sec:visualization:routing-tables:

Visualizing Routing Tables
~~~~~~~~~~~~~~~~~~~~~~~~~~

In a complex network topology, it is difficult to see how a packet would be
routed because the relevant data is scattered among network nodes and hidden in
their routing tables. INET contains support for visualization of routing tables
and can display routing information graphically in a concise way. Using
visualization, it is often possible to understand routing in a simulation
without looking into individual routing tables. The visualization currently
supports IPv4.

The :ned:`RoutingTableVisualizer` module (included in the network as part of
:ned:`IntegratedVisualizer`) is responsible for visualizing routing table
entries.

The visualizer basically annotates network links with labeled arrows that
connect source nodes to next hop nodes. The module visualizes those routing
table entries that participate in the routing of a given set of destination
addresses, by default the addresses of all interfaces of all nodes in the
network. That is, it selects the best (longest prefix) matching routes for all
destination addresses from each routing table and shows them as arrows that
point to the next hop. Note that one arrow might need to represent several
routing entries, for example, when distinct prefixes are routed towards the same
next hop.

Routing table entries are visually represented by solid arrows. An arrow going
from a source node represents a routing table entry in the source node's routing
table, and the endpoint node of the arrow is the next hop in the visualized
routing table entry. By default, the routing entry is displayed on the arrows in
the following format:

destination/mask -> gateway (interface)

The format can be changed by setting the visualizer's :par:`labelFormat`
parameter.

Filtering is also possible. The :par:`nodeFilter` parameter controls which
nodes' routing tables should be visualized (by default, all nodes), and the
:par:`destinationFilter` parameter selects the set of destination nodes to
consider (again, by default all nodes).

The visualizer reacts to changes, such as when a routing protocol changes a
routing entry or an IP address gets assigned to an interface by DHCP. The
visualizer automatically updates the visualizations according to the specified
filters. This is very useful for the simulation of mobile ad-hoc networks.

.. _ug:sec:visualization:displaying-ip-addresses-and-other-interface-information:

Displaying IP Addresses and Other Interface Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the simulation of complex networks, it is often useful to be able to
display node IP addresses, interface names, etc. above the node icons or
on the links. For example, when automatic address assignment is used in
a hierarchical network (e.g. using :ned:`Ipv4NetworkConfigurator`),
visual inspection can help to verify that the result matches the
expectations. While it is true that addresses and other interface data
can also be accessed in the GUI by diving into the interface tables of
each node, that is tedious, and unsuitable for getting an overview.

The :ned:`InterfaceTableVisualizer` module (included in the network as
part of :ned:`IntegratedVisualizer`) displays data about network nodes'
interfaces. (Interfaces are contained in interface tables, hence the
name.) By default, the visualization is turned off. When it is enabled
using the :par:`displayInterfaceTables` parameter, the default is that
interface names, IP addresses and netmask length are displayed, above
the nodes (for wireless interfaces) and on the links (for wired
interfaces). By clicking on an interface label, details are displayed in
the inspector panel.

The visualizer has several configuration parameters. The :par:`format`
parameter specifies what information is displayed about interfaces. It
takes a format string, which can contain the following directives:

-  %N: interface name

-  %4: IPv4 address

-  %6: IPv6 address

-  %n: network address. This is either the IPv4 or the IPv6 address

-  %l: netmask length

-  %M: MAC address

-  %\: conditional newline for wired interfaces. The '\' needs to be
   escaped with another '\', i.e. '%\\'

-  %i and %s: the info() and str() functions for the networkInterface
   class, respectively

The default format string is ``"%N %\\%n/%l"``, i.e. interface name, IP
address and netmask length.

The set of visualized interfaces can be selected with the configurator's
:par:`nodeFilter` and :par:`interfaceFilter` parameters. By default, all
interfaces of all nodes are visualized, except for loopback addresses
(the default for the :par:`interfaceFilter` parameter is ``"not lo\"``.)

It is possible to display the labels for wired interfaces above the node
icons, instead of on the links. This can be done by setting the
:par:`displayWiredInterfacesAtConnections` parameter to false.

There are also several parameters for styling, such as color and font
selection.

.. _ug:sec:visualization:ieee-80211-network-membership:

Visualizing IEEE 802.11 Network Membership
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When simulating Wi-Fi networks that overlap in space, it is difficult to see
which node is a member of which network. The membership may even change over
time. It would be useful to be able to display the SSID above node icons.

IEEE 802.11 network membership can be visualized by including an
:ned:`Ieee80211Visualizer` module in the simulation. Adding an
:ned:`IntegratedVisualizer` is also an option, as it also contains an
:ned:`Ieee80211Visualizer`. Displaying network membership is disabled by
default, but it can be enabled by setting the visualizer's
:par:`displayAssociations` parameter to true.

The :ned:`Ieee80211Visualizer` displays an icon and the SSID above network nodes
that are part of a Wi-Fi network. The icons are color-coded according to the
SSID. The icon, colors, and other visual properties can be configured via
parameters of the visualizer.

The visualizer's :par:`nodeFilter` parameter selects which nodes' memberships
are visualized. The :par:`interfaceFilter` parameter selects which interfaces
are considered in the visualization. By default, all interfaces of all nodes are
considered.

.. _ug:sec:visualization:transport-connections:

Visualizing Transport Connections
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In a large network with a complex topology, it can be difficult to determine which nodes
are communicating and if any communication is occurring at all. Visualizing transport
connections provides an easy way to obtain information about active transport connections
in the network. It allows for easy identification and differentiation of connections based
on their endpoints, providing a quick overview of the number of connections in individual
nodes and the entire network.

The :ned:`TransportConnectionVisualizer` module (also part of :ned:`IntegratedVisualizer`)
displays color-coded icons above the two endpoints of an active, established transport
layer connection. The icons appear when the connection is established and disappear when
it is closed. Multiple connections at a node result in multiple icons. Icons for the same
connection have the same color at both endpoints. Additionally, letter codes can be displayed
to help identify connections. Note that this visualizer does not display the paths of packets.
For path visualization, refer to the :ned:`TransportRouteVisualizer` covered in the
section :ref:`ug:sec:visualization:transport-path-activity`.

The visualization is disabled by default, but it can be enabled by setting the visualizer's
:par:`displayTransportConnections` parameter to true.

Connections to be visualized can be filtered using the :par:`sourcePortFilter`,
:par:`destinationPortFilter`, :par:`sourceNodeFilter`, and :par:`destinationNodeFilter`
parameters.

The icons and other visual properties can be customized using the visualizer's parameters.

.. _ug:sec:visualization:the-infrastructure:

Visualizing The Infrastructure
------------------------------

.. _ug:sec:visualization:the-physical-environment:

Visualizing the Physical Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The physical environment significantly impacts the communication of wireless devices.
Objects like walls within buildings limit mobility and obstruct radio signals, often
causing packet loss. To better understand simulations, it is crucial to visualize the
physical objects present in the environment.

The visualization of physical objects is achieved using the :ned:`PhysicalEnvironmentVisualizer`
(also part of :ned:`IntegratedVisualizer`). The objects themselves are provided by the
PhysicalEnvironment module, defining their geometry, physical properties, and visual appearance
in the XML configuration of the PhysicalEnvironment module.

The two-dimensional projection of physical objects is determined by the :ned:`SceneCanvasVisualizer`
module. The default view is a top view (z-axis), but side views (x and y axes) or isometric and
orthographic projections can also be configured.

For 3D rendering using the OpenSceneGraph (OSG) library, the simulation and OMNeT++ both
need to be compiled with OSG support. The 3D view can be activated using the Qtenv toolbar.

.. _ug:sec:visualization:node-mobility:

Visualizing Node Mobility
~~~~~~~~~~~~~~~~~~~~~~~~~

In INET simulations, the movement of mobile nodes is often as important as the
communication among them. However, as mobile nodes roam, it is often difficult
to visually follow their movement. INET provides a visualizer that not only
makes visually tracking mobile nodes easier but also indicates other properties
like speed and direction.

Node mobility can be visualized by the :ned:`MobilityVisualizer` module
(included in the network as part of :ned:`IntegratedVisualizer`). By default,
mobility visualization is enabled, but it can be disabled by setting the
:par:`displayMovements` parameter to false.

By default, all mobilities are considered for the visualization. This selection
can be narrowed with the visualizer's :par:`moduleFilter` parameter.

The visualizer has several important features:

- Movement Trail: It displays a line along the recent path of movements. The
  trail gradually fades out as time passes. Color, trail length, and other
  graphical properties can be changed with parameters of the visualizer.

- Velocity Vector: Velocity is visually represented by an arrow. Its starting
  point is the node, and its direction coincides with the movement's direction.
  The arrow's length is proportional to the node's speed.

- Orientation Arc: Node orientation is represented by an arc whose size is
  specified by the :par:`orientationArcSize` parameter. This value is the
  relative size of the arc compared to a full circle. The default arc size is
  0.25, meaning a quarter of a circle.

These features are disabled by default but can be enabled by setting the
visualizer's :par:`displayMovementTrails`, :par:`displayVelocities`, and
:par:`displayOrientations` parameters to true.

