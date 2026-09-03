%% 元数据类
classdef Metadata

    properties (Access = public)
        Header (1,1) string = "qREST_DATA";
        Version (1,3) double = [1, 0, 0];
        Units (1,2) string = ["m", "s"];

        ProjectName (1,1) string = "";
        GeoLocation (1,3) double = [0, 0, 0];
        StructuralType (1,1) string = "";
        StructuralFootprint (1,1) StructuralFootprint = StructuralFootprint();
        ElevationNum (1,1) double {mustBePositive} = 1;
        Elevation (:,1) double = zeros(0, 1);

        Provider (1,1) string = "";
        ChannelNum (1,1) double {mustBePositive} = 1;
        ChannelNo (:,1) double = zeros(0, 1);
        ChannelID (:,1) string = strings(0, 1);
        DeviceType (:,1) string = strings(0, 1);
        Direction (:,1) string = strings(0, 1);
        Measurand (:,1) string = strings(0, 1);
        Scale(:,1) double = zeros(0, 1);
        Azimuth (:,1) double = zeros(0, 1);
        LocationXYZ (:,3) double = zeros(0, 3);

        EventName (1,1) string = "";
        StartTime (1,1) datetime = datetime;
        NPTS (1,1) double {mustBePositive} = 1;
        SamplingFrequency (1,1) double {mustBePositive} = 10;
        DT (1,1) double {mustBePositive} = 0.1;
        Corrected (1,1) string = "";
    end

    methods (Access = public)
        function obj = Metadata(varargin)
            if nargin == 0
                return;
            elseif nargin == 1 && isa(varargin{1}, 'struct')
                metadata_struct = varargin{1};
            else
                error('Invalid input. Please provide a struct to initialize the Metadata object.');
            end

            if isfield(metadata_struct, 'Header'); obj.Header = metadata_struct.Header; end
            if isfield(metadata_struct, 'Version'); obj.Version = metadata_struct.Version; end
            if isfield(metadata_struct, 'Units'); obj.Units = metadata_struct.Units; end

            if isfield(metadata_struct, 'BuildingInfo')
                obj.ProjectName = string(metadata_struct.BuildingInfo.ProjectName);
                obj.GeoLocation = [metadata_struct.BuildingInfo.GeoLocation.Longitude, ...
                    metadata_struct.BuildingInfo.GeoLocation.Latitude, ...
                    metadata_struct.BuildingInfo.GeoLocation.NorthAngle];
                obj.StructuralType = string(metadata_struct.BuildingInfo.StructuralType);
                obj.StructuralFootprint = StructuralFootprint(metadata_struct.BuildingInfo.StructuralFootprint);
                obj.ElevationNum = metadata_struct.BuildingInfo.ElevationNum;
                obj.Elevation = metadata_struct.BuildingInfo.Elevation(:);
            end

            if isfield(metadata_struct, 'InstrumentInfo')
                obj.Provider = string(metadata_struct.InstrumentInfo.Provider);
                obj.ChannelNum = metadata_struct.InstrumentInfo.ChannelNum;
                obj.ChannelNo = zeros(obj.ChannelNum, 1);
                obj.ChannelID = strings(obj.ChannelNum, 1);
                obj.DeviceType = strings(obj.ChannelNum, 1);
                obj.Direction = strings(obj.ChannelNum, 1);
                obj.Measurand = strings(obj.ChannelNum, 1);
                obj.Scale = zeros(obj.ChannelNum, 1);
                obj.LocationXYZ = zeros(obj.ChannelNum, 3);
                obj.Azimuth = zeros(obj.ChannelNum, 1);

                for i = 1:obj.ChannelNum
                    channel = metadata_struct.InstrumentInfo.Channels(i);
                    obj.ChannelNo(i) = channel.ChannelNo;
                    obj.ChannelID(i) = string(channel.ChannelID);
                    if isfield(channel, 'DeviceType'); obj.DeviceType(i) = string(channel.DeviceType); end
                    if isfield(channel, 'Direction'); obj.Direction(i) = string(channel.Direction); end
                    obj.Measurand(i) = string(channel.Measurand);
                    obj.Scale(i) = channel.Scale;
                    obj.LocationXYZ(i, :) = reshape(channel.LocationXYZ, 1, 3);
                    obj.Azimuth(i) = channel.Azimuth;
                end
            end

            if isfield(metadata_struct, 'DataInfo')
                obj.EventName = string(metadata_struct.DataInfo.EventName);
                obj.DT = metadata_struct.DataInfo.DT;
                obj.SamplingFrequency = 1 / obj.DT;
                if isfield(metadata_struct.DataInfo, 'StartTime') && ~isempty(metadata_struct.DataInfo.StartTime)
                    obj.StartTime = Metadata.parseStartTime(metadata_struct.DataInfo.StartTime);
                end
                obj.NPTS = metadata_struct.DataInfo.NPTS;
                obj.Corrected = string(metadata_struct.DataInfo.Corrected);
            end
        end

        function instrument_heights = getInstrumentHeights(obj)
            heights = sort(obj.LocationXYZ(:, 3));
            instrument_heights = uniquetol(heights, 0.001, 'DataScale', 1);
        end

        function channel_idxs = getChannelsAtHeight(obj, target_height)
            tol = 0.001;
            channel_idxs = find(abs(obj.LocationXYZ(:, 3) - target_height) < tol);
        end

        function channel_idxs = getChannelsAtHeightByDirection(obj, target_height, direction)
            all_idx = obj.getChannelsAtHeight(target_height);
            channel_idxs = [];
            for k = 1:numel(all_idx)
                idx = all_idx(k);
                ch_dir = obj.classifyChannelDirection(obj.Azimuth(idx));
                if direction == "HORIZONTAL"
                    if any(ch_dir == ["X", "Y", "HORIZONTAL"])
                        channel_idxs(end + 1, 1) = idx; %#ok<AGROW>
                    end
                elseif ch_dir == direction
                    channel_idxs(end + 1, 1) = idx; %#ok<AGROW>
                end
            end
        end

        function info = getLayerDataInfo(obj, target_height)
            horizontal_idx = obj.getChannelsAtHeightByDirection(target_height, "HORIZONTAL");
            x_idx = obj.getChannelsAtHeightByDirection(target_height, "X");
            y_idx = obj.getChannelsAtHeightByDirection(target_height, "Y");
            z_idx = obj.getChannelsAtHeightByDirection(target_height, "Z");
            all_idx = obj.getChannelsAtHeight(target_height);

            horizontal_J = obj.makeHorizontalJacobian(horizontal_idx);
            xy_J = Metadata.selectColumns(horizontal_J, [1, 2]);
            x_J = obj.makeHorizontalJacobian(x_idx);
            xr_J = Metadata.selectColumns(x_J, [1, 3]);
            y_J = obj.makeHorizontalJacobian(y_idx);
            yr_J = Metadata.selectColumns(y_J, [2, 3]);
            z_J = obj.makeVerticalJacobian(z_idx);

            horizontal_rank = Metadata.matrixRank(horizontal_J);
            xy_rank = Metadata.matrixRank(xy_J);
            xr_rank = Metadata.matrixRank(xr_J);
            yr_rank = Metadata.matrixRank(yr_J);
            z_rank = Metadata.matrixRank(z_J);

            if z_rank >= 3 && isempty(horizontal_idx)
                info = obj.makeLayerInfo(target_height, "SP_BOTTOM_R", ["Z", "RX", "RY"], z_idx, x_idx, y_idx, z_idx, z_J);
            elseif ~isempty(z_idx) && xy_rank >= 2 && horizontal_rank < 3
                info = obj.makeLayerInfo(target_height, "XYZ_DIRECTION", ["X", "Y", "Z"], [horizontal_idx; z_idx], x_idx, y_idx, z_idx, obj.makeXYZJacobian(horizontal_idx, z_idx));
            elseif horizontal_rank >= 3 && obj.hasXComponent(horizontal_idx) && obj.hasYComponent(horizontal_idx)
                info = obj.makeLayerInfo(target_height, "XYR_DIRECTION", ["X", "Y", "RZ"], horizontal_idx, x_idx, y_idx, z_idx, horizontal_J);
            elseif xr_rank >= 2 && ~isempty(x_idx) && isempty(y_idx) && isempty(z_idx)
                info = obj.makeLayerInfo(target_height, "MX_DIRECTION", ["X", "RZ"], x_idx, x_idx, y_idx, z_idx, xr_J);
            elseif yr_rank >= 2 && ~isempty(y_idx) && isempty(x_idx) && isempty(z_idx)
                info = obj.makeLayerInfo(target_height, "MY_DIRECTION", ["Y", "RZ"], y_idx, x_idx, y_idx, z_idx, yr_J);
            elseif xy_rank >= 2 && horizontal_rank < 3 && isempty(z_idx)
                info = obj.makeLayerInfo(target_height, "XY_DIRECTION", ["X", "Y"], horizontal_idx, x_idx, y_idx, z_idx, xy_J);
            elseif Metadata.matrixRank(Metadata.selectColumns(x_J, 1)) >= 1 && isempty(y_idx) && isempty(z_idx)
                info = obj.makeLayerInfo(target_height, "X_DIRECTION", "X", x_idx, x_idx, y_idx, z_idx, Metadata.selectColumns(x_J, 1));
            elseif Metadata.matrixRank(Metadata.selectColumns(y_J, 2)) >= 1 && isempty(x_idx) && isempty(z_idx)
                info = obj.makeLayerInfo(target_height, "Y_DIRECTION", "Y", y_idx, x_idx, y_idx, z_idx, Metadata.selectColumns(y_J, 2));
            elseif ~isempty(z_idx) && isempty(horizontal_idx)
                info = obj.makeLayerInfo(target_height, "Z_DIRECTION", "Z", z_idx, x_idx, y_idx, z_idx, Metadata.selectColumns(z_J, 1));
            elseif horizontal_rank >= 1 && isempty(x_idx) && isempty(y_idx) && isempty(z_idx)
                info = obj.makeLayerInfo(target_height, "H_DIRECTION", "H", horizontal_idx, x_idx, y_idx, z_idx, xy_J);
            else
                info = obj.makeLayerInfo(target_height, "UNKNOWN", strings(0, 1), all_idx, x_idx, y_idx, z_idx, []);
            end
        end

        structure_fig = plotStructure(obj)
        [J, channel_idx] = getFloorJacobian(obj, target_height)
    end

    methods (Access = private)
        function J = makeHorizontalJacobian(obj, idx)
            J = zeros(numel(idx), 3);
            for i = 1:numel(idx)
                k = idx(i);
                x = obj.LocationXYZ(k, 1);
                y = obj.LocationXYZ(k, 2);
                az = obj.Azimuth(k);
                nx = sind(az);
                ny = cosd(az);
                J(i, :) = [nx, ny, x * ny - y * nx];
            end
        end

        function J = makeVerticalJacobian(obj, idx)
            J = zeros(numel(idx), 3);
            for i = 1:numel(idx)
                k = idx(i);
                x = obj.LocationXYZ(k, 1);
                y = obj.LocationXYZ(k, 2);
                J(i, :) = [1, y, -x];
            end
        end

        function J = makeXYZJacobian(obj, horizontal_idx, z_idx)
            J = zeros(numel(horizontal_idx) + numel(z_idx), 3);
            for i = 1:numel(horizontal_idx)
                k = horizontal_idx(i);
                az = obj.Azimuth(k);
                J(i, 1) = sind(az);
                J(i, 2) = cosd(az);
            end
            for i = 1:numel(z_idx)
                J(numel(horizontal_idx) + i, 3) = 1;
            end
        end

        function tf = hasXComponent(obj, idx)
            tf = false;
            for i = 1:numel(idx)
                az = obj.Azimuth(idx(i));
                if obj.classifyChannelDirection(az) == "X" || abs(sind(az)) > 1e-9
                    tf = true;
                    return;
                end
            end
        end

        function tf = hasYComponent(obj, idx)
            tf = false;
            for i = 1:numel(idx)
                az = obj.Azimuth(idx(i));
                if obj.classifyChannelDirection(az) == "Y" || abs(cosd(az)) > 1e-9
                    tf = true;
                    return;
                end
            end
        end

        function info = makeLayerInfo(~, height, rank_name, dofs, channel_idx, x_idx, y_idx, z_idx, J)
            info = struct();
            info.height = height;
            info.rank = string(rank_name);
            info.observable_dofs = string(dofs(:));
            info.channel_indices = channel_idx(:);
            info.x_indices = x_idx(:);
            info.y_indices = y_idx(:);
            info.z_indices = z_idx(:);
            info.jacobian = J;
        end
    end

    methods (Static)
        function direction = classifyChannelDirection(azimuth)
            if abs(azimuth + 1.0) < 1e-9
                direction = "Z";
            elseif Metadata.angularDistance(azimuth, 90.0) <= 5.0 || Metadata.angularDistance(azimuth, 270.0) <= 5.0
                direction = "X";
            elseif Metadata.angularDistance(azimuth, 0.0) <= 5.0 || Metadata.angularDistance(azimuth, 180.0) <= 5.0
                direction = "Y";
            elseif isfinite(azimuth)
                direction = "HORIZONTAL";
            else
                direction = "UNSUPPORTED";
            end
        end

        function d = angularDistance(a, b)
            diff_val = abs(mod(a, 360) - mod(b, 360));
            d = min(diff_val, 360 - diff_val);
        end

        function selected = selectColumns(source, columns)
            if isempty(source)
                selected = zeros(size(source, 1), numel(columns));
                return;
            end
            selected = source(:, columns);
        end

        function r = matrixRank(A)
            if isempty(A)
                r = 0;
            else
                r = rank(A, 1e-9);
            end
        end

        function t = parseStartTime(value)
            value = string(value);
            formats = ["yyyy-MM-dd'T'HH:mm:ss.SSSZZZZZ", "yyyy-MM-dd'T'HH:mm:ss.SSSXXX", "yyyy-MM-dd HH:mm:ss"];
            for i = 1:numel(formats)
                try
                    t = datetime(value, 'InputFormat', formats(i), 'TimeZone', 'Asia/Hong_Kong');
                    return;
                catch
                end
            end
            t = datetime(value, 'TimeZone', 'Asia/Hong_Kong');
        end
    end
end

%% 在 3D 中绘制结构平面轮廓与各标高层
function structure_fig = plotStructure(obj)
structure_fig = figure('Name', 'Structural Layout & Sensor Map', 'Color', 'w');
hold on; grid on; view(3);

% 从 StructuralFootprint 中提取边界框用于绘图
bbox = obj.StructuralFootprint.BoundingBox;
maxX = bbox(1); minX = bbox(2);
maxY = bbox(3); minY = bbox(4);

% 1. 在每个标高层绘制结构平面轮廓
elevations = obj.Elevation;
for i = 1:length(elevations)
    z = elevations(i);
    if obj.StructuralFootprint.Shape == "Rectangular"
        X = [minX, maxX, maxX, minX, minX];
        Y = [minY, minY, maxY, maxY, minY];
        Z = [z, z, z, z, z];
        plot3(X, Y, Z, 'k-', 'LineWidth', 1.0, 'Color', [0.7 0.7 0.7]);
    end
end

% 2. 绘制建筑四角柱的边界线
if obj.StructuralFootprint.Shape == "Rectangular"
    z_min = min(elevations); z_max = max(elevations);
    plot3([minX, minX], [minY, minY], [z_min, z_max], 'k-', 'LineWidth', 1.5);
    plot3([maxX, maxX], [minY, minY], [z_min, z_max], 'k-', 'LineWidth', 1.5);
    plot3([maxX, maxX], [maxY, maxY], [z_min, z_max], 'k-', 'LineWidth', 1.5);
    plot3([minX, minX], [maxY, maxY], [z_min, z_max], 'k-', 'LineWidth', 1.5);
end

% 3. 将传感器位置绘制为红色点
locs = obj.LocationXYZ;
scatter3(locs(:,1), locs(:,2), locs(:,3), 50, 'r', 'filled', 'MarkerEdgeColor', 'k');

% 4. 将传感器朝向绘制为蓝色箭头
% 动态计算箭头可视长度，约取建筑短边的 10%
arrow_len = min((maxX - minX), (maxY - minY)) * 0.1;
if arrow_len <= 0; arrow_len = 2; end % 当建筑很小时保证箭头仍可见的最小长度

% Azimuth convention: 0 deg points to +Y; positive rotation is clockwise.
u = sind(obj.Azimuth) * arrow_len;
v = cosd(obj.Azimuth) * arrow_len;
w = zeros(size(u));

% 使用 quiver3 绘制表示传感器朝向的箭头
quiver3(locs(:,1), locs(:,2), locs(:,3), u, v, w, 0, ...
    'LineWidth', 2, 'MaxHeadSize', 0.5, 'Color', 'b');

% 5. 设置坐标轴标签、标题及等比例显示
xlabel('X (m)', 'FontWeight', 'bold');
ylabel('Y (m)', 'FontWeight', 'bold');
zlabel('Elevation (m)', 'FontWeight', 'bold');
title(sprintf('Project: %s\nStructural Layout & Sensor Map', obj.ProjectName), 'Interpreter', 'none');
axis equal;
end
